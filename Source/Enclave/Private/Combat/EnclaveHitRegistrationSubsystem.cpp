// Copyright Lightcarrier Studios

#include "Combat/EnclaveHitRegistrationSubsystem.h"

#include "Combat/Components/EnclaveCombatPolicyComponent.h"
#include "Combat/EnclaveShotIntent.h"
#include "Combat/EnclaveHistoricalCapsuleIntersection.h"
#include "Combat/EnclaveHistoricalHitQuery.h"
#include "Combat/EnclaveServerHistoricalEvaluator.h"
#include "Combat/EnclaveSSRTimingCalibration.h"
#include "Combat/EnclaveSSRTimingPolicy.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"
#include "Characters/EnclaveCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Controllers/EnclavePlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Debug/EnclaveDebug.h"
#if ENCLAVE_INCLUDE_DEBUG_CODE
#include "HAL/PlatformTime.h"
#endif

#if ENCLAVE_INCLUDE_DEBUG_CODE
// Timing sensitivity sweep (calibration tool for the anchor offset). Deliberately
// bounded to five samples; all results are diagnostic, not presentation proof.
static TAutoConsoleVariable<bool> CVarDebugSSRTimingSweep(
    TEXT("Enclave.Debug.SSRTimingSweep"), false,
    TEXT("Evaluate 5 bounded history samples starting at server RTT anchor, shadow only; never affects damage."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarDebugSSRTimingSweepStepMs(
    TEXT("Enclave.Debug.SSRTimingSweepStepMs"), 25.f,
    TEXT("Step between timing sweep samples in ms [0,100]. Results outside the rewind window are unavailable, not clamped."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarDebugSSRTimingSweepStartMs(
    TEXT("Enclave.Debug.SSRTimingSweepStartMs"), 0.f,
    TEXT("Offset from the server RTT anchor of the first sweep sample in ms [-150,100]. Negative rewinds less than a full RTT. Shadow only."), ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDebugSSRSelection(
    TEXT("Enclave.Debug.SSRSelection"),
    false,
    TEXT("Log the authoritative D11D-4 SSR selection for every validated remote shot."),
    ECVF_Cheat);
#endif

static TAutoConsoleVariable<bool> CVarDebugFireTrace(
	TEXT("Enclave.Debug.FireTrace"),
	false,
	TEXT("Draw authoritative hitscan rays and log blocking hits."),
	ECVF_Cheat);

bool UEnclaveHitRegistrationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// For a UWorldSubsystem, Outer is its owning UWorld. Skip remote client worlds.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->GetNetMode() != NM_Client;
}

// Stage B conservative limits. These compare to CURRENT shooter state; they
// deliberately allow divergence from network latency, animation and movement.
// Precise temporal validation belongs to historical shooter-pose recording.
namespace EnclaveShotValidation
{
	constexpr double MaxOriginSeparationCm = 225.0;
	constexpr double MaxDirectionDeltaDegrees = 110.0;
	constexpr double MaxShotAgeSeconds = 1.5;
	constexpr double MaxFutureLeadSeconds = 0.25;
}

bool UEnclaveHitRegistrationSubsystem::ValidateClientShotIntent(const FEnclaveShotIntent& Intent,
	const AActor* Shooter, uint16 ExpectedShotSequence, FEnclaveValidatedShotRay& OutValidatedRay,
	FString& OutRejectionReason) const
{
	OutValidatedRay = FEnclaveValidatedShotRay();
	OutRejectionReason.Reset();

	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !IsValid(Shooter) || !Shooter->HasAuthority() ||
		Shooter->GetWorld() != World)
	{
		OutRejectionReason = TEXT("missing authoritative shooter/world");
		return false;
	}
	if (Intent.Origin.ContainsNaN() || Intent.Direction.ContainsNaN() ||
		!FMath::IsFinite(Intent.ClientServerTimeSeconds) ||
		!FMath::IsNearlyEqual(Intent.Direction.SizeSquared(), 1.0, 0.03))
	{
		OutRejectionReason = TEXT("invalid shot geometry or timestamp");
		return false;
	}
	if (Intent.ShotSequence != ExpectedShotSequence || Intent.SpreadSeed != 0)
	{
		OutRejectionReason = TEXT("shot sequence mismatch or unsupported spread seed");
		return false;
	}

	const AGameStateBase* GameState = World->GetGameState();
	if (!IsValid(GameState))
	{
		OutRejectionReason = TEXT("server clock unavailable");
		return false;
	}
	const double ServerReceiptTimeSeconds = GameState->GetServerWorldTimeSeconds();
	const double DeltaSeconds = ServerReceiptTimeSeconds - Intent.ClientServerTimeSeconds;
	if (DeltaSeconds > EnclaveShotValidation::MaxShotAgeSeconds ||
		DeltaSeconds < -EnclaveShotValidation::MaxFutureLeadSeconds)
	{
		OutRejectionReason = FString::Printf(TEXT("implausible shot time (age=%.3fs)"), DeltaSeconds);
		return false;
	}

	FVector ServerEye;
	FRotator ServerRotation;
	Shooter->GetActorEyesViewPoint(ServerEye, ServerRotation);
	const double OriginDeltaCm = FVector::Distance(ServerEye, FVector(Intent.Origin));
	const FVector ClientDirection = FVector(Intent.Direction).GetSafeNormal();
	const double Dot = FMath::Clamp(FVector::DotProduct(ServerRotation.Vector(), ClientDirection), -1.0, 1.0);
	const double AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
	if (OriginDeltaCm > EnclaveShotValidation::MaxOriginSeparationCm)
	{
		OutRejectionReason = FString::Printf(TEXT("shot origin too far from shooter (%.1fcm)"), OriginDeltaCm);
		return false;
	}
	if (AngleDegrees > EnclaveShotValidation::MaxDirectionDeltaDegrees)
	{
		OutRejectionReason = FString::Printf(TEXT("shot direction outside present-time tolerance (%.1fdeg)"), AngleDegrees);
		return false;
	}

	// Do not accept a synthetic camera origin reachable only through blocking
	// geometry from the server's current eye. This is conservative and can
	// reject a legitimate corner peek until historical shooter poses exist.
	if (OriginDeltaCm > 10.0)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EnclaveShotOriginValidation), true);
		Params.AddIgnoredActor(Shooter);
		TArray<AActor*> AttachedActors;
		Shooter->GetAttachedActors(AttachedActors);
		Params.AddIgnoredActors(AttachedActors);
		FHitResult Obstruction;
		const bool bWeaponBlocked = World->LineTraceSingleByChannel(Obstruction, ServerEye, FVector(Intent.Origin), ECC_Weapon, Params);
		if (bWeaponBlocked)
		{
			OutRejectionReason = TEXT("blocking geometry between server eye and shot origin");
			return false;
		}
	}

	OutValidatedRay.Origin = FVector(Intent.Origin);
	OutValidatedRay.Direction = ClientDirection;
	OutValidatedRay.ClientServerTimeSeconds = Intent.ClientServerTimeSeconds;
	OutValidatedRay.ShotSequence = Intent.ShotSequence;
	OutValidatedRay.TimingTarget = Intent.TimingTarget;
	OutValidatedRay.TimingStampMs = Intent.TimingStampMs;
	OutValidatedRay.SSRGameplayClaim = Intent.SSRGameplayClaim; // Diagnostic, untrusted
	OutValidatedRay.Timing.ClientEstimatedShotTimeSeconds = Intent.ClientServerTimeSeconds;
	OutValidatedRay.Timing.ServerReceiptTimeSeconds = ServerReceiptTimeSeconds;
	if (const APawn* ShooterPawn = Cast<APawn>(Shooter))
	{
		if (const APlayerState* ShooterPS = ShooterPawn->GetPlayerState())
		{
			const double ServerPingMs = ShooterPS->GetPingInMilliseconds();
			if (FMath::IsFinite(ServerPingMs) && ServerPingMs >= 0.0)
			{
				// UE's PlayerState ping is a server-observed RTT estimate, not a
				// trusted measure of downlink latency or mesh interpolation.
				OutValidatedRay.Timing.ServerObservedRTTSeconds = ServerPingMs / 1000.0;
			}
		}
	}
	if (const UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(World))
	{
		OutValidatedRay.Timing.MaxRewindSeconds = CombatPolicy->GetMaxRewindSeconds();
	}
	// The C4 patrol trajectory inverse fit is debug-only and does not resolve
	// trustworthy presentation timing. Keep authoritative traces present-time.
	return true;
}

bool UEnclaveHitRegistrationSubsystem::RegisterHitscan(const FEnclaveHitscanQuery& Query, FHitResult& OutHit) const
{
	OutHit = FHitResult();
	const UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || !Query.Instigator.IsValid() ||
		Query.Start.ContainsNaN() || Query.End.ContainsNaN() || Query.Start.Equals(Query.End))
	{
		return false;
	}

	// Character collision policy and strategy selection MUST use the same
	// authoritative combat policy. A missing policy is a safe Capsule fallback.
	const UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(World);
	const EEnclaveWeaponHitDetectionMode Mode = CombatPolicy
		? CombatPolicy->GetWeaponHitDetectionMode()
		: EEnclaveWeaponHitDetectionMode::Capsule;

	// Listen-host, AI and other authority-local shots have no validated client
	// timing and stay present-time, as does every shot while SSR is inactive.
	FEnclaveSSRSelectionDecision Decision;
	Decision.Selection = EEnclaveSSRSelection::PresentTimeByMatchPolicy;
	if (CombatPolicy && CombatPolicy->IsServerSideRewindActive() && Query.bHasValidatedClientTiming)
	{
		Decision = SelectHistoricalHit(Query, *CombatPolicy, OutHit);
	}

	bool bHit = false;
	switch (Decision.Selection)
	{
	case EEnclaveSSRSelection::Historical:
		bHit = Decision.bSelectedHit;
		break;
	case EEnclaveSSRSelection::NoHitUnavailable:
		OutHit = FHitResult();
		break;
	default:
		bHit = TraceCurrentGeometry(Query, OutHit, Mode);
		break;
	}
#if ENCLAVE_INCLUDE_DEBUG_CODE
	DebugSweepSSRTimingSensitivity(Query, Mode);
	DebugCompareSSRGameplay(Query, bHit, OutHit, Mode);
#endif
	return bHit;
}

namespace
{
// A historical capsule hit as a regular blocking FHitResult. The component is
// the target's live capsule; the location is where it was at the rewind time.
FHitResult MakeHistoricalHitResult(const FEnclaveHitscanQuery& Query, const FEnclaveServerHistoricalAssessment& Assessment)
{
	AEnclaveCharacter* Target = Assessment.Target.Get();
	const FVector ShotDirection = (Query.End - Query.Start).GetSafeNormal();
	const FVector Normal = FEnclaveHistoricalCapsuleIntersection::SurfaceNormal(
		Assessment.Capsule, Assessment.Point, -ShotDirection);
	FHitResult Hit(Target, Target ? Target->GetCapsuleComponent() : nullptr, Assessment.Point, Normal);
	Hit.bBlockingHit = true;
	Hit.TraceStart = Query.Start;
	Hit.TraceEnd = Query.End;
	Hit.Distance = static_cast<float>(Assessment.EntryDistanceCm);
	const double Length = FVector::Dist(Query.Start, Query.End);
	Hit.Time = Length > UE_DOUBLE_SMALL_NUMBER ? static_cast<float>(Assessment.EntryDistanceCm / Length) : 0.0f;
	return Hit;
}
}

FEnclaveSSRSelectionDecision UEnclaveHitRegistrationSubsystem::SelectHistoricalHit(const FEnclaveHitscanQuery& Query,
	const UEnclaveCombatPolicyComponent& CombatPolicy, FHitResult& OutHit) const
{
	FEnclaveSSRSelectionDecision Decision;
	UWorld* World = GetWorld();
	AActor* Shooter = Query.Instigator.Get();
	const FEnclaveShotTimingContext& Timing = Query.ValidatedClientTiming;
	const FEnclaveSSRTimingSettings Settings = CombatPolicy.GetSSRTimingSettings();

	// Echoed evidence counts only if it names a frame this server recorded for
	// that exact target. It never names the hit target: the trace still does.
	const AEnclaveCharacter* StampTarget = Cast<AEnclaveCharacter>(Query.TimingTarget.Get());
	double StampTime = 0.0;
	bool bStampVerified = false;
	if (IsValid(StampTarget) && StampTarget != Shooter && StampTarget->GetWorld() == World)
	{
		if (const UEnclaveTargetHistoryComponent* History = StampTarget->FindComponentByClass<UEnclaveTargetHistoryComponent>())
		{
			bStampVerified = History->TryResolveRecordedStampMs(Query.TimingStampMs, StampTime);
		}
	}

	const FEnclaveSSRTimingResolution Resolution = ResolveSSRTiming(Settings, Timing, bStampVerified, StampTime);
	FEnclaveServerHistoricalAssessment Assessment;
	if (Resolution.bValid)
	{
		const bool bOverride = Resolution.bUseStamp && Settings.Source == EEnclaveSSRTimingSource::Hybrid;
		Assessment = FEnclaveServerHistoricalEvaluator::EvaluateCapsules(World, Shooter, Query.Start, Query.End,
			Timing, Resolution.BaseTimeSeconds, bOverride ? StampTarget : nullptr, Resolution.StampTimeSeconds);
	}
	else
	{
		Assessment.Policy.Outcome = EEnclaveSSRQueryOutcome::Unavailable;
		Assessment.Policy.UnavailableReason = Resolution.UnavailableReason;
	}

	switch (Assessment.Policy.Outcome)
	{
	case EEnclaveSSRQueryOutcome::HistoricalHit:
		Decision.Selection = EEnclaveSSRSelection::Historical;
		Decision.bSelectedHit = Assessment.Target.IsValid();
		if (Decision.bSelectedHit)
		{
			OutHit = MakeHistoricalHitResult(Query, Assessment);
		}
		break;
	case EEnclaveSSRQueryOutcome::HistoricalMiss:
		// Terminal: never retry present-time after a definitive historical miss
		// (including world-occluded targets). The bullet still stops at the wall.
		Decision.Selection = EEnclaveSSRSelection::Historical;
		Decision.bSelectedHit = Assessment.bWorldBlocked;
		if (Decision.bSelectedHit)
		{
			OutHit = Assessment.WorldHit;
		}
		break;
	default:
		Decision.UnavailableReason = Assessment.Policy.UnavailableReason;
		Decision.Selection = CombatPolicy.GetSSRUnavailableFallbackPolicy() == EEnclaveSSRUnavailableFallbackPolicy::RejectHit
			? EEnclaveSSRSelection::NoHitUnavailable
			: EEnclaveSSRSelection::PresentTimeUnavailableFallback;
		break;
	}

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (CVarDebugSSRSelection.GetValueOnGameThread())
	{
		const double Receipt = Timing.ServerReceiptTimeSeconds;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRSelection SERVER seq=%u source=%s selection=%d hit=%d target=%s outcome=%d reason=%d occluded=%d "
			"stampTarget=%s stampVerified=%d stampUsed=%d stampRejected=%d baseAgeMs=%.1f stampAgeMs=%.1f rttMs=%.1f "
			"missing=%d",
			static_cast<uint32>(Query.ShotSequence), *UEnum::GetValueAsString(Settings.Source),
			static_cast<int32>(Decision.Selection),
			Assessment.Policy.Outcome == EEnclaveSSRQueryOutcome::HistoricalHit && Assessment.Target.IsValid() ? 1 : 0,
			*GetNameSafe(Assessment.Target.Get()), static_cast<int32>(Assessment.Policy.Outcome),
			static_cast<int32>(Assessment.Policy.UnavailableReason), Assessment.Policy.bWorldOccluded ? 1 : 0,
			*GetNameSafe(StampTarget), bStampVerified ? 1 : 0, Resolution.bUseStamp ? 1 : 0,
			Resolution.bStampRejected ? 1 : 0,
			Resolution.bValid ? (Receipt - Resolution.BaseTimeSeconds) * 1000.0 : -1.0,
			bStampVerified ? (Receipt - StampTime) * 1000.0 : -1.0,
			Timing.ServerObservedRTTSeconds * 1000.0, Assessment.MissingHistoryCount);
	}
#endif
	return Decision;
}

bool UEnclaveHitRegistrationSubsystem::TraceCurrentGeometry(const FEnclaveHitscanQuery& Query, FHitResult& OutHit, EEnclaveWeaponHitDetectionMode Mode) const
{
	UWorld* World = GetWorld();
	if (!World) { return false; }

	// Current strategies share a ray, but NOT a target representation:
	//   Capsule          -> character capsule blocks ECC_Weapon
	//   SkeletalHitboxes -> character mesh Physics Asset blocks ECC_Weapon
	// Character::ApplyWeaponHitDetectionCollisionPolicy configures that representation.
	// TODO: When the modes require different query algorithms, dispatch here to
	// per-strategy implementations rather than adding weapon-specific branches.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnclaveHitRegistration), true);
	AActor* Instigator = Query.Instigator.Get();
	Params.AddIgnoredActor(Instigator);
	TArray<AActor*> AttachedActors;
	Instigator->GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);

	const bool bHit = World->LineTraceSingleByChannel(OutHit, Query.Start, Query.End, ECC_Weapon, Params);
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (AEnclavePlayerController::IsShotAlignmentDebugEnabled())
	{
		if (const APawn* ShooterPawn = Cast<APawn>(Instigator))
		{
			if (AEnclavePlayerController* ShooterController = Cast<AEnclavePlayerController>(ShooterPawn->GetController()))
			{
				ShooterController->DebugRecordAuthoritativeTrace(Query.Start, Query.End, bHit, OutHit.ImpactPoint);
			}
		}
	}
#endif
	if (CVarDebugFireTrace.GetValueOnGameThread())
	{
		DrawDebugLine(World, Query.Start, bHit ? OutHit.ImpactPoint : Query.End,
			bHit ? FColor::Green : FColor::Red, false, 2.0f, 0, 1.0f);
		ENCLAVE_LOG(LogEnclaveCombat, Log, "Hit registration mode=%s hit=%s component=%s bone=%s",
			GetEnclaveWeaponHitDetectionModeName(Mode), *GetNameSafe(OutHit.GetActor()),
			*GetNameSafe(OutHit.GetComponent()), *OutHit.BoneName.ToString());
	}
	return bHit;
}

#if ENCLAVE_INCLUDE_DEBUG_CODE
void UEnclaveHitRegistrationSubsystem::DebugSweepSSRTimingSensitivity(
    const FEnclaveHitscanQuery& Query, EEnclaveWeaponHitDetectionMode Mode) const
{
    if (!CVarDebugSSRTimingSweep.GetValueOnGameThread() ||
        !Query.bHasValidatedClientTiming)
    {
        return;
    }
    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_Client ||
        Mode != EEnclaveWeaponHitDetectionMode::Capsule || !Query.Instigator.IsValid())
    {
        return;
    }

    const FEnclaveShotTimingContext& Timing = Query.ValidatedClientTiming;
    const double StepMs = static_cast<double>(CVarDebugSSRTimingSweepStepMs.GetValueOnGameThread());
    if (!FMath::IsFinite(StepMs) || StepMs <= 0.0 || StepMs > 100.0)
    {
        ENCLAVE_LOG(LogEnclaveCombat, Warning,
            "SSRTimingSweep SERVER seq=%u rejected=invalid_step stepMs=%.2f (SHADOW ONLY)",
            static_cast<uint32>(Query.ShotSequence), StepMs);
        return;
    }

    const double StartMs = FMath::Clamp(
        static_cast<double>(CVarDebugSSRTimingSweepStartMs.GetValueOnGameThread()), -150.0, 100.0);
    EnclaveSSRTimingCalibration::FSummary Summary;
    FString Outcomes;
    for (int32 Index = 0; Index < EnclaveSSRTimingCalibration::SampleCount; ++Index)
    {
        const double StepOffsetMs = EnclaveSSRTimingCalibration::DelayMs(Index, StepMs);
        const double AdditionalMs = StartMs + StepOffsetMs;
        double CandidateTime = 0.0;
        EEnclaveSSRQueryOutcome Outcome = EEnclaveSSRQueryOutcome::Unavailable;
        if (StepOffsetMs >= 0.0 &&
            Timing.TryResolveServerAnchoredHistoryTime(AdditionalMs / 1000.0, CandidateTime))
        {
            const FEnclaveServerHistoricalAssessment Assessment =
                FEnclaveServerHistoricalEvaluator::EvaluateCapsules(
                    World, Query.Instigator.Get(), Query.Start, Query.End, Timing, CandidateTime);
            Outcome = Assessment.Policy.Outcome;
        }
        Summary.Add(Index, Outcome);
        if (!Outcomes.IsEmpty()) { Outcomes += TEXT(","); }
        Outcomes += FString::Printf(TEXT("%.0f:%d"), AdditionalMs, static_cast<int32>(Outcome));
    }

    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRTimingSweep SERVER seq=%u rttMs=%.1f stepMs=%.1f sampleOutcomes=%s hits=%d misses=%d unavailable=%d transitions=%d firstTransitionIndex=%d "
        "(SERVER RTT ESTIMATE PLUS ADDITIONAL DELAY; NOT VERIFIED CLIENT PRESENTATION; SHADOW ONLY; NO DAMAGE AUTHORITY)",
        static_cast<uint32>(Query.ShotSequence), Timing.ServerObservedRTTSeconds * 1000.0, StepMs, *Outcomes,
        Summary.Hits, Summary.Misses, Summary.Unavailable, Summary.Transitions,
        Summary.FirstTransitionIndex);

}

void UEnclaveHitRegistrationSubsystem::DebugCompareSSRGameplay(const FEnclaveHitscanQuery& Query,
	bool bLiveHit, const FHitResult& LiveHit, EEnclaveWeaponHitDetectionMode Mode) const
{
	if (!AEnclavePlayerController::IsSSRGameplayGateEnabled() ||
		!Query.bHasValidatedClientTiming || Query.SSRGameplayClaim.Classification == 0)
	{
		return;
	}

	const FEnclaveSSRGameplayClaim& Claim = Query.SSRGameplayClaim;
	const AEnclaveCharacter* Target = Cast<AEnclaveCharacter>(Claim.Target.Get());
	const FEnclaveShotTimingContext& Timing = Query.ValidatedClientTiming;
	UWorld* World = GetWorld();
	const AActor* Shooter = Query.Instigator.Get();
	const bool bLiveOnClaimedTarget = bLiveHit && LiveHit.GetActor() == Target;
	if (!World || !IsValid(Shooter) || !IsValid(Target) || Target == Shooter ||
		Target->GetWorld() != World || Mode != EEnclaveWeaponHitDetectionMode::Capsule ||
		Claim.Classification < 1 || Claim.Classification > 4)
	{
		// An invalid target/category cannot be scored as a valid test sample.
		// Report it explicitly: the analysis gate must NOT silently drop it.
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRGameplay SERVER key=%u status=invalid_claim category=%d scenario=%d unresolved=1 (shadow; NO DAMAGE AUTHORITY)",
			Query.ShotSequence, static_cast<int32>(Claim.Classification), static_cast<int32>(Claim.Scenario));
		return;
	}

	const double Stamp = Claim.EchoedMovementStamp;
	const double Age = Timing.ServerReceiptTimeSeconds - Stamp;
	const double RTT = Timing.ServerObservedRTTSeconds;
	// Only the authoritative-history stamp is evaluated. The previously tested
	// client-elapsed challenger has been retired from the D11C shadow contract.
	constexpr double RTTMarginSeconds = 0.050;
	const bool bStampPlausible = FMath::IsFinite(Stamp) && Stamp > 0.0 &&
		FMath::IsFinite(RTT) && RTT >= 0.0 &&
		Age >= 0.0 && Age <= Timing.MaxRewindSeconds &&
		Age <= RTT + RTTMarginSeconds;
	if (!bStampPlausible)
	{
		// Server-only stationary fallback remains conservative. No client scenario
		// classification is sufficient to authorize this exception.
		bool bVerifiedStationary = false;
		if (Stamp == 0.0)
		{
			if (const UEnclaveTargetHistoryComponent* StationaryHistory = Target->FindComponentByClass<UEnclaveTargetHistoryComponent>())
			{
				double Oldest = 0.0;
				double Newest = 0.0;
				FEnclaveTargetCapsuleFrame OldFrame;
				FEnclaveTargetCapsuleFrame MidFrame;
				FEnclaveTargetCapsuleFrame NewFrame;
				if (StationaryHistory->GetHistoryTimeBounds(Oldest, Newest) && Newest - Oldest >= 0.10 &&
					StationaryHistory->TryGetCapsuleAtTime(Newest - 0.10, OldFrame) &&
					StationaryHistory->TryGetCapsuleAtTime(Newest - 0.05, MidFrame) &&
					StationaryHistory->TryGetCapsuleAtTime(Newest, NewFrame))
				{
					bVerifiedStationary = FVector::Dist(OldFrame.Center, NewFrame.Center) <= 1.0 &&
						FVector::Dist(MidFrame.Center, NewFrame.Center) <= 1.0 &&
						FMath::Abs(OldFrame.RadiusCm - NewFrame.RadiusCm) <= 0.5f &&
						FMath::Abs(OldFrame.HalfHeightCm - NewFrame.HalfHeightCm) <= 0.5f &&
						FVector::Dist(NewFrame.Center, Target->GetCapsuleComponent()->GetComponentLocation()) <= 1.0 &&
						Target->GetVelocity().Size() <= 5.0;
				}
			}
		}
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRGameplay SERVER key=%u status=%s stampStatus=%s category=%d scenario=%d ageMs=%.2f rttMs=%.1f liveTargetHit=%d stampAvailable=0 stampFinalHit=%d stampFallback=1 stationaryVerified=%d (PRESENT fallback; SHADOW ONLY)",
			Query.ShotSequence, bVerifiedStationary ? TEXT("stationary_present") : TEXT("rejected_time"),
			bVerifiedStationary ? TEXT("stationary_present") : TEXT("rejected_time"),
			static_cast<int32>(Claim.Classification), static_cast<int32>(Claim.Scenario),
			Age * 1000.0, RTT * 1000.0, bLiveOnClaimedTarget ? 1 : 0,
			bLiveOnClaimedTarget ? 1 : 0, bVerifiedStationary ? 1 : 0);
		return;
	}

	const UEnclaveTargetHistoryComponent* History = Target->FindComponentByClass<UEnclaveTargetHistoryComponent>();
	FEnclaveTargetCapsuleFrame BaseFrame;
	const bool bBase = IsValid(History) && History->TryGetCapsuleAtTime(Stamp, BaseFrame);
	double RecordedStamp = 0.0;
	const bool bCapturedServerStamp = IsValid(History) &&
		History->TryResolveRecordedStampMs(EnclaveSSRTimingPolicy::QuantizeServerTimeMs(Stamp), RecordedStamp);
	if (!bBase)
	{
		double HistoryOldest = 0.0;
		double HistoryNewest = 0.0;
		const bool bHasHistoryBounds = IsValid(History) &&
			History->GetHistoryTimeBounds(HistoryOldest, HistoryNewest);
		const double NewestDeltaMs = bHasHistoryBounds ? (Stamp - HistoryNewest) * 1000.0 : 0.0;
		const TCHAR* HistoryReason = !bHasHistoryBounds ? TEXT("empty") :
			(Stamp > HistoryNewest ? TEXT("newer_than_latest") :
			(Stamp < HistoryOldest ? TEXT("older_than_oldest") : TEXT("inside_window_unavailable")));
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRHistoryBoundary key=%u target=%s reason=%s stamp=%.9f oldest=%.9f newest=%.9f aheadOfNewestMs=%.3f serverReceipt=%.9f ageMs=%.3f frames=%d (DIAGNOSTIC ONLY)",
			Query.ShotSequence, *GetNameSafe(Target), HistoryReason, Stamp,
			HistoryOldest, HistoryNewest, NewestDeltaMs, Timing.ServerReceiptTimeSeconds,
			Age * 1000.0, IsValid(History) ? History->GetHistorySampleCount() : 0);
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRGameplay SERVER key=%u status=missing_history stampStatus=missing_history category=%d scenario=%d ageMs=%.2f liveTargetHit=%d stampAvailable=0 stampFinalHit=%d stampFallback=1 (PRESENT fallback; SHADOW ONLY)",
			Query.ShotSequence, static_cast<int32>(Claim.Classification), static_cast<int32>(Claim.Scenario),
			Age * 1000.0, bLiveOnClaimedTarget ? 1 : 0, bLiveOnClaimedTarget ? 1 : 0);
		return;
	}

	if (!bCapturedServerStamp)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRStampProvenance SERVER key=%u recorded=0 deliveryVerified=0 (ECHO NOT AUTHENTICATED; SHADOW ONLY)", Query.ShotSequence);
	}
	const double QueryStartSeconds = FPlatformTime::Seconds();
	FEnclaveHistoricalHitQuery Historical(World, Query.Start, Query.End, Query.Instigator.Get());
	if (!Historical.IsPrepared())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRGameplay SERVER key=%u status=query_unavailable stampStatus=query_unavailable category=%d scenario=%d liveTargetHit=%d stampAvailable=0 stampFinalHit=%d stampFallback=1 (PRESENT fallback; SHADOW ONLY)",
			Query.ShotSequence, static_cast<int32>(Claim.Classification), static_cast<int32>(Claim.Scenario),
			bLiveOnClaimedTarget ? 1 : 0, bLiveOnClaimedTarget ? 1 : 0);
		return;
	}
	const FEnclaveHistoricalHitQueryResult StampResult = Historical.Trace(Stamp, Target);
	const double CPUms = (FPlatformTime::Seconds() - QueryStartSeconds) * 1000.0;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRGameplay SERVER key=%u status=measured stampStatus=measured claimTarget=%s category=%d scenario=%d ageMs=%.2f rttMs=%.1f stampAvailable=1 stampHit=%d liveTargetHit=%d stampFinalHit=%d stampFallback=0 stampOccluded=%d worldBlocked=%d shadowCpuMs=%.3f (NO DAMAGE AUTHORITY)",
		Query.ShotSequence, *GetNameSafe(Target), static_cast<int32>(Claim.Classification), static_cast<int32>(Claim.Scenario),
		Age * 1000.0, RTT * 1000.0, StampResult.bHit ? 1 : 0,
		bLiveOnClaimedTarget ? 1 : 0, StampResult.bHit ? 1 : 0,
		StampResult.bOccluded ? 1 : 0, Historical.IsWorldBlocked() ? 1 : 0, CPUms);
}
#endif

