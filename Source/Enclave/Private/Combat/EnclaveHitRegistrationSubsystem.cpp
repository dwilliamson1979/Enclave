// Copyright Lightcarrier Studios

#include "Combat/EnclaveHitRegistrationSubsystem.h"

#include "Game/EnclaveGameModeBase.h"
#include "Combat/EnclaveShotIntent.h"
#include "Combat/EnclaveHistoricalCapsuleIntersection.h"
#include "Combat/EnclaveHistoricalHitQuery.h"
#include "Combat/EnclaveServerHistoricalEvaluator.h"
#include "Combat/EnclaveSSRTimingCalibration.h"
#include "Combat/EnclaveSSRTemporalUncertainty.h"
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

static TAutoConsoleVariable<bool> CVarDebugSSRPreflight(
	TEXT("Enclave.Debug.SSRPreflight"),
	false,
	TEXT("Stage C0 only: independently measure server-vs-client shooter ray, reported shot age and RTT, and comparison of weapon vs world-only origin obstruction. Also logs one nearby simulated-proxy mesh smoothing sample per local shot. Does not alter hit validation, hitscan, or rewind."),
	ECVF_Cheat);

bool UEnclaveHitRegistrationSubsystem::IsSSRPreflightDebugEnabled()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	return CVarDebugSSRPreflight.GetValueOnGameThread();
#else
	return false;
#endif
}

#if ENCLAVE_INCLUDE_DEBUG_CODE
// D11D-3 integration exercise: server-chosen *test* age, not an assertion
// about what a remote player rendered. Debug-only and never a damage selector.
static TAutoConsoleVariable<bool> CVarDebugSSRHistoricalProbe(
    TEXT("Enclave.Debug.SSRHistoricalProbe"), false,
    TEXT("Run isolated historical evaluator alongside real remote shots using a controlled server test age. Shadow only."),
    ECVF_Cheat);
static TAutoConsoleVariable<float> CVarDebugSSRHistoricalProbeAgeMs(
    TEXT("Enclave.Debug.SSRHistoricalProbeAgeMs"), 50.0f,
    TEXT("Server-controlled historical sample age in milliseconds for debug evaluator exercise (0..200 by default). Not target presentation time."),
    ECVF_Cheat);

// D11D-3C: compare candidate clocks in SHADOW ONLY. Neither is trusted
// presentation time, even when a historical query returns a definitive hit.
static TAutoConsoleVariable<bool> CVarDebugSSRTimingCandidates(
    TEXT("Enclave.Debug.SSRTimingCandidates"), false,
    TEXT("Evaluate server RTT anchor vs untrusted shot-clock candidate in shadow; never enables SSR damage."),
    ECVF_Cheat);
static TAutoConsoleVariable<float> CVarDebugSSRTimingSmoothingMs(
    TEXT("Enclave.Debug.SSRTimingSmoothingMs"), 0.f,
    TEXT("Additional assumed target presentation delay in milliseconds for SHADOW candidate comparison only."),
    ECVF_Cheat);

// D11D-3C: independent server-only timing sensitivity sweep. Deliberately
// bounded to five samples; all results are diagnostic, not presentation proof.
static TAutoConsoleVariable<bool> CVarDebugSSRTimingSweep(
    TEXT("Enclave.Debug.SSRTimingSweep"), false,
    TEXT("Evaluate 5 bounded history samples starting at server RTT anchor, shadow only; never affects damage."), ECVF_Cheat);
static TAutoConsoleVariable<float> CVarDebugSSRTimingSweepStepMs(
    TEXT("Enclave.Debug.SSRTimingSweepStepMs"), 25.f,
    TEXT("Step between timing sweep samples in ms [0,100]. Results outside the rewind window are unavailable, not clamped."), ECVF_Cheat);

static TAutoConsoleVariable<bool> CVarDebugSSRProductionReadiness(
    TEXT("Enclave.Debug.SSRProductionReadiness"),
    false,
    TEXT("D11D-3B: audit validated remote shot readiness for production historical evaluation. "
         "No history-time authority exists yet; never queries historical hits or changes damage."),
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
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (AEnclavePlayerController::IsSSRClockDebugEnabled())
	{
		const APawn* ShooterPawn = Cast<APawn>(Shooter);
		const APlayerState* ShooterPS = ShooterPawn ? ShooterPawn->GetPlayerState() : nullptr;
		const double RTTMs = IsValid(ShooterPS) ? ShooterPS->GetPingInMilliseconds() : -1.0;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRClockIntent SERVER key=%u intentShotTime=%.6f serverReceiptWorld=%.6f serverReceiptProcess=%.6f observedShotAgeMs=%+.1f playerStateRTTMs=%.1f (REAL GAS INTENT; timestamp UNTRUSTED; no authority change)",
			ExpectedShotSequence, Intent.ClientServerTimeSeconds, ServerReceiptTimeSeconds,
			FPlatformTime::Seconds(), DeltaSeconds * 1000.0, RTTMs);
	}
#endif
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
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (IsSSRPreflightDebugEnabled())
	{
		const APawn* ShooterPawn = Cast<APawn>(Shooter);
		const APlayerState* ShooterPS = ShooterPawn ? ShooterPawn->GetPlayerState() : nullptr;
		const double PingMs = ShooterPS ? ShooterPS->GetPingInMilliseconds() : -1.0;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRPreflight SERVER shooter=%s key=%u originDeltaCm=%.2f aimDeltaDeg=%.2f clientTimestampAgeMs=%.1f playerStateRTTMs=%.1f shooterSpeedCmS=%.1f serverEye=%s clientOrigin=%s serverAim=%s clientAim=%s (PRESENT-TIME comparison; RTT is an estimate)",
			*GetNameSafe(Shooter), ExpectedShotSequence, OriginDeltaCm, AngleDegrees,
			DeltaSeconds * 1000.0, PingMs, Shooter->GetVelocity().Size(),
			*ServerEye.ToCompactString(), *FVector(Intent.Origin).ToCompactString(),
			*ServerRotation.Vector().ToCompactString(), *ClientDirection.ToCompactString());
	}
#endif
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
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (IsSSRPreflightDebugEnabled())
		{
			// SHADOW QUERY ONLY. Compare the existing Weapon-channel decision with
			// world-object geometry excluding Pawns. This deliberately does not
			// affect Stage B validity, nor claim both channels are equivalent.
			FCollisionQueryParams WorldParams(SCENE_QUERY_STAT(EnclaveShotOriginWorldProbe), true);
			WorldParams.AddIgnoredActor(Shooter);
			WorldParams.AddIgnoredActors(AttachedActors);
			for (TActorIterator<APawn> It(World); It; ++It)
			{
				WorldParams.AddIgnoredActor(*It);
			}
			FCollisionObjectQueryParams WorldObjects;
			WorldObjects.AddObjectTypesToQuery(ECC_WorldStatic);
			WorldObjects.AddObjectTypesToQuery(ECC_WorldDynamic);
			FHitResult WorldObstruction;
			const bool bWorldBlocked = World->LineTraceSingleByObjectType(WorldObstruction,
				ServerEye, FVector(Intent.Origin), WorldObjects, WorldParams);
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRPreflight OBSTRUCTION key=%u weaponBlocked=%d weaponActor=%s weaponIsPawn=%d worldObjectBlocked=%d worldActor=%s disagreement=%d (world probe ignores Pawns and includes WorldStatic+WorldDynamic; DIAGNOSTIC ONLY)",
				ExpectedShotSequence, bWeaponBlocked ? 1 : 0, *GetNameSafe(Obstruction.GetActor()),
				Cast<APawn>(Obstruction.GetActor()) ? 1 : 0, bWorldBlocked ? 1 : 0,
				*GetNameSafe(WorldObstruction.GetActor()), bWeaponBlocked != bWorldBlocked ? 1 : 0);
		}
#endif
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
	if (const AEnclaveGameModeBase* GameMode = World->GetAuthGameMode<AEnclaveGameModeBase>())
	{
		OutValidatedRay.Timing.MaxRewindSeconds = GameMode->GetMaxRewindSeconds();
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
	// authoritative GameMode state. Missing GameMode is a safe Capsule fallback.
	const AEnclaveGameModeBase* GameMode = World->GetAuthGameMode<AEnclaveGameModeBase>();
	const EEnclaveWeaponHitDetectionMode Mode = GameMode
		? GameMode->GetWeaponHitDetectionMode()
		: EEnclaveWeaponHitDetectionMode::Capsule;

	const bool bCurrentHit = TraceCurrentGeometry(Query, OutHit, Mode);
#if ENCLAVE_INCLUDE_DEBUG_CODE
	DebugAuditServerHistoricalReadiness(Query, Mode);
	DebugCompareSSRTimingCandidates(Query, Mode);
	DebugSweepSSRTimingSensitivity(Query, Mode);
	DebugProbeServerHistoricalEvaluator(Query, Mode);
	DebugCompareHistoricalCapsules(Query, bCurrentHit, OutHit, Mode);
	DebugCompareSSRGameplay(Query, bCurrentHit, OutHit, Mode);
#endif
	return bCurrentHit;
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
void UEnclaveHitRegistrationSubsystem::DebugProbeServerHistoricalEvaluator(
    const FEnclaveHitscanQuery& Query, EEnclaveWeaponHitDetectionMode Mode) const
{
    if (!CVarDebugSSRHistoricalProbe.GetValueOnGameThread() ||
        !Query.bHasValidatedClientTiming)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!IsValid(World) || World->GetNetMode() == NM_Client ||
        !Query.Instigator.IsValid())
    {
        return;
    }

    const FEnclaveShotTimingContext& Timing = Query.ValidatedClientTiming;
    // Debug-only fixed-age fixture controlled by the server operator.
    // This is NOT the shooter-visible target time. Never forward the result
    // to the authoritative FHitResult or gameplay damage pipeline.
    const float RequestedAgeMs = CVarDebugSSRHistoricalProbeAgeMs.GetValueOnGameThread();
    const bool bAgeValid = FMath::IsFinite(RequestedAgeMs) && RequestedAgeMs >= 0.f &&
        FMath::IsFinite(Timing.MaxRewindSeconds) &&
        static_cast<double>(RequestedAgeMs) <= Timing.MaxRewindSeconds * 1000.0;
    if (!bAgeValid)
    {
        ENCLAVE_LOG(LogEnclaveCombat, Log,
            "SSRHistoricalProbe SERVER seq=%u status=invalid_fixture_age requestedMs=%.2f (NO DAMAGE AUTHORITY)",
            static_cast<uint32>(Query.ShotSequence), RequestedAgeMs);
        return;
    }

    const double FixtureTime = Timing.ServerReceiptTimeSeconds -
        static_cast<double>(RequestedAgeMs) / 1000.0;
    const FEnclaveServerHistoricalAssessment Assessment =
        FEnclaveServerHistoricalEvaluator::EvaluateCapsules(
            World, Query.Instigator.Get(), Query.Start, Query.End,
            Timing, FixtureTime);
    const bool bHistoricalHit = Assessment.Policy.Outcome == EEnclaveSSRQueryOutcome::HistoricalHit;
    const bool bHistoricalMiss = Assessment.Policy.Outcome == EEnclaveSSRQueryOutcome::HistoricalMiss;
    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRHistoricalProbe SERVER seq=%u fixtureAgeMs=%.2f hit=%d miss=%d unavailable=%d reason=%d "
        "available=%d missing=%d occluded=%d target=%s distanceCm=%.2f "
        "(CONTROLLED SERVER TEST TIME; NOT PRESENTATION TIME; NO DAMAGE AUTHORITY)",
        static_cast<uint32>(Query.ShotSequence), RequestedAgeMs,
        bHistoricalHit ? 1 : 0, bHistoricalMiss ? 1 : 0,
        Assessment.Policy.Outcome == EEnclaveSSRQueryOutcome::Unavailable ? 1 : 0,
        static_cast<int32>(Assessment.Policy.UnavailableReason),
        Assessment.AvailableHistoryCount, Assessment.MissingHistoryCount,
        Assessment.Policy.bWorldOccluded ? 1 : 0,
        *GetNameSafe(Assessment.Target.Get()), Assessment.EntryDistanceCm);
}

void UEnclaveHitRegistrationSubsystem::DebugCompareSSRTimingCandidates(
    const FEnclaveHitscanQuery& Query, EEnclaveWeaponHitDetectionMode Mode) const
{
    if (!CVarDebugSSRTimingCandidates.GetValueOnGameThread() ||
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
    const float SmoothingMs = CVarDebugSSRTimingSmoothingMs.GetValueOnGameThread();
    const double SmoothingSeconds = static_cast<double>(SmoothingMs) / 1000.0;
    double ServerAnchor = 0.0;
    double ClientClockCandidate = 0.0;
    const bool bServerValid = Timing.TryResolveServerDerivedHistoryTime(
        SmoothingSeconds, ServerAnchor);
    const bool bClientValid = Timing.TryResolveTargetHistoryTime(
        SmoothingSeconds, ClientClockCandidate);

    FEnclaveServerHistoricalAssessment ServerAssessment;
    FEnclaveServerHistoricalAssessment ClientAssessment;
    if (bServerValid)
    {
        ServerAssessment = FEnclaveServerHistoricalEvaluator::EvaluateCapsules(
            World, Query.Instigator.Get(), Query.Start, Query.End, Timing, ServerAnchor);
    }
    if (bClientValid)
    {
        ClientAssessment = FEnclaveServerHistoricalEvaluator::EvaluateCapsules(
            World, Query.Instigator.Get(), Query.Start, Query.End, Timing, ClientClockCandidate);
    }
    const bool bBothValid = bServerValid && bClientValid;
    const bool bComparable = bBothValid &&
        ServerAssessment.Policy.HasDefinitiveHistoricalResult() &&
        ClientAssessment.Policy.HasDefinitiveHistoricalResult();
    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRTimingCandidates SERVER seq=%u assumedSmoothingMs=%.2f "
        "serverAnchorValid=%d untrustedShotClockValid=%d "
        "candidateDeltaMs=%.2f serverOutcome=%d shotClockOutcome=%d "
        "serverReason=%d shotClockReason=%d comparable=%d outcomesAgree=%d "
        "(TWO UNPROVEN CANDIDATES; CLIENT CLOCK UNTRUSTED; SHADOW ONLY; NO DAMAGE AUTHORITY)",
        static_cast<uint32>(Query.ShotSequence), SmoothingMs,
        bServerValid ? 1 : 0, bClientValid ? 1 : 0,
        bBothValid ? (ServerAnchor - ClientClockCandidate) * 1000.0 : 0.0,
        static_cast<int32>(ServerAssessment.Policy.Outcome),
        static_cast<int32>(ClientAssessment.Policy.Outcome),
        static_cast<int32>(ServerAssessment.Policy.UnavailableReason),
        static_cast<int32>(ClientAssessment.Policy.UnavailableReason),
        bComparable ? 1 : 0,
        bComparable && ServerAssessment.Policy.Outcome == ClientAssessment.Policy.Outcome ? 1 : 0);
}

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

    EnclaveSSRTimingCalibration::FSummary Summary;
    EnclaveSSRTemporalUncertainty::FAssessment Uncertainty;
    FString Outcomes;
    for (int32 Index = 0; Index < EnclaveSSRTimingCalibration::SampleCount; ++Index)
    {
        const double AdditionalMs = EnclaveSSRTimingCalibration::DelayMs(Index, StepMs);
        double CandidateTime = 0.0;
        EEnclaveSSRQueryOutcome Outcome = EEnclaveSSRQueryOutcome::Unavailable;
        if (AdditionalMs >= 0.0 &&
            Timing.TryResolveServerDerivedHistoryTime(AdditionalMs / 1000.0, CandidateTime))
        {
            const FEnclaveServerHistoricalAssessment Assessment =
                FEnclaveServerHistoricalEvaluator::EvaluateCapsules(
                    World, Query.Instigator.Get(), Query.Start, Query.End, Timing, CandidateTime);
            Outcome = Assessment.Policy.Outcome;
        }
        Summary.Add(Index, Outcome);
        Uncertainty.Add(Outcome);
        if (!Outcomes.IsEmpty()) { Outcomes += TEXT(","); }
        Outcomes += FString::Printf(TEXT("%.0f:%d"), AdditionalMs, static_cast<int32>(Outcome));
    }

    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRTimingSweep SERVER seq=%u stepMs=%.1f sampleOutcomes=%s hits=%d misses=%d unavailable=%d transitions=%d firstTransitionIndex=%d "
        "(SERVER RTT ESTIMATE PLUS ADDITIONAL DELAY; NOT VERIFIED CLIENT PRESENTATION; SHADOW ONLY; NO DAMAGE AUTHORITY)",
        static_cast<uint32>(Query.ShotSequence), StepMs, *Outcomes,
        Summary.Hits, Summary.Misses, Summary.Unavailable, Summary.Transitions,
        Summary.FirstTransitionIndex);

    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRTemporalUncertainty SERVER seq=%u classification=%d samples=%d hits=%d misses=%d unavailable=%d "
        "presentationProvenance=0 damageAuthorized=%d "
        "(STABILITY IS NOT PRESENTATION PROOF; SHADOW ONLY; NO DAMAGE AUTHORITY)",
        static_cast<uint32>(Query.ShotSequence),
        static_cast<int32>(Uncertainty.Classification),
        Uncertainty.Samples, Uncertainty.Hits, Uncertainty.Misses, Uncertainty.Unavailable,
        Uncertainty.IsAuthorizedForDamage() ? 1 : 0);
}

void UEnclaveHitRegistrationSubsystem::DebugAuditServerHistoricalReadiness(
    const FEnclaveHitscanQuery& Query, EEnclaveWeaponHitDetectionMode Mode) const
{
    if (!CVarDebugSSRProductionReadiness.GetValueOnGameThread() ||
        !Query.bHasValidatedClientTiming)
    {
        return;
    }

    // This method must remain a read-only audit. A validated shot timestamp
    // establishes shot plausibility, NOT the time at which the target was rendered.
    // Neither the RTT nor the D11C target-specific diagnostic claim is an
    // authorized replacement for that missing presentation-time evidence.
    const UWorld* World = GetWorld();
    const AEnclaveGameModeBase* GameMode = World
        ? World->GetAuthGameMode<AEnclaveGameModeBase>() : nullptr;
    const FEnclaveShotTimingContext& Timing = Query.ValidatedClientTiming;
    const TCHAR* Reason = TEXT("presentation_time_unavailable");
    if (!GameMode)
    {
        Reason = TEXT("game_mode_unavailable");
    }
    else if (Mode != EEnclaveWeaponHitDetectionMode::Capsule)
    {
        Reason = TEXT("unsupported_geometry");
    }
    else if (!FMath::IsFinite(Timing.ServerReceiptTimeSeconds) ||
             Timing.ServerReceiptTimeSeconds <= 0.0 ||
             !FMath::IsFinite(Timing.MaxRewindSeconds) ||
             Timing.MaxRewindSeconds <= 0.0)
    {
        Reason = TEXT("server_timing_unavailable");
    }

    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRProductionReadiness SERVER seq=%u ready=0 reason=%s "
        "receipt=%.6f clientShotAgeMs=%.2f observedRTTMs=%.2f "
        "rewindLimitMs=%.2f (NO HISTORICAL QUERY; NO DAMAGE AUTHORITY)",
        static_cast<uint32>(Query.ShotSequence), Reason,
        Timing.ServerReceiptTimeSeconds,
        Timing.GetObservedShotAgeSeconds() * 1000.0,
        Timing.ServerObservedRTTSeconds * 1000.0,
        Timing.MaxRewindSeconds * 1000.0);
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
		Claim.Classification < 1 || Claim.Classification > 3)
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
	const bool bCapturedServerStamp = IsValid(History) && History->HasRecordedFrameStamp(Stamp);
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
		"SSRGameplay SERVER key=%u status=measured stampStatus=measured category=%d scenario=%d ageMs=%.2f rttMs=%.1f stampAvailable=1 stampHit=%d liveTargetHit=%d stampFinalHit=%d stampFallback=0 stampOccluded=%d worldBlocked=%d shadowCpuMs=%.3f (NO DAMAGE AUTHORITY)",
		Query.ShotSequence, static_cast<int32>(Claim.Classification), static_cast<int32>(Claim.Scenario),
		Age * 1000.0, RTT * 1000.0, StampResult.bHit ? 1 : 0,
		bLiveOnClaimedTarget ? 1 : 0, StampResult.bHit ? 1 : 0,
		StampResult.bOccluded ? 1 : 0, Historical.IsWorldBlocked() ? 1 : 0, CPUms);
}
#endif

#if ENCLAVE_INCLUDE_DEBUG_CODE
static TAutoConsoleVariable<int32> CVarDebugSSRShadow(
	TEXT("Enclave.Debug.SSRShadow"),
	0,
	TEXT("Stage C4D: 0=off, 1=shadow-compare live hits with historical capsule rays at both the client-timestamp baseline and server-receipt-minus-observed-RTT baseline (both assume 0ms additional smoothing). Capsule mode only. Never grants hits or damage."),
	ECVF_Cheat);

void UEnclaveHitRegistrationSubsystem::DebugCompareHistoricalCapsules(const FEnclaveHitscanQuery& Query,
	bool bLiveHit, const FHitResult& LiveHit, EEnclaveWeaponHitDetectionMode Mode) const
{
	if (CVarDebugSSRShadow.GetValueOnGameThread() == 0 || !Query.bHasValidatedClientTiming)
	{
		return;
	}

	// Capsule-only history must not be compared with skeletal Physics Asset hits.
	if (Mode != EEnclaveWeaponHitDetectionMode::Capsule)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRShadow SERVER key=%u skipped=unsupported_skeletal_mode (SHADOW ONLY; no gameplay effect)",
			Query.ShotSequence);
		return;
	}

	UWorld* World = GetWorld();
	AActor* Shooter = Query.Instigator.Get();
	if (!World || World->GetNetMode() == NM_Client || !IsValid(Shooter))
	{
		return;
	}

	const FEnclaveShotTimingContext& Timing = Query.ValidatedClientTiming;
	// C4D deliberately tests zero additional smoothing: this is a baseline,
	// NEVER a claim that the remote player actually sees a zero-delay mesh.
	constexpr double DiagnosticAdditionalSmoothingSeconds = 0.0;
	double ClientBaselineTime = 0.0;
	double ServerBaselineTime = 0.0;
	const bool bClientTimeValid = Timing.TryResolveTargetHistoryTime(0.0, ClientBaselineTime);
	const bool bServerTimeValid = Timing.TryResolveServerDerivedHistoryTime(
		DiagnosticAdditionalSmoothingSeconds, ServerBaselineTime);

	const TCHAR* ServerTimeStatus = TEXT("eligible");
	if (!bServerTimeValid)
	{
		if (!FMath::IsFinite(Timing.ServerObservedRTTSeconds) || Timing.ServerObservedRTTSeconds < 0.0)
		{
			ServerTimeStatus = TEXT("no_server_rtt");
		}
		else if (FMath::IsFinite(Timing.MaxRewindSeconds) &&
			Timing.ServerObservedRTTSeconds + DiagnosticAdditionalSmoothingSeconds > Timing.MaxRewindSeconds)
		{
			ServerTimeStatus = TEXT("outside_rewind_window");
		}
		else
		{
			ServerTimeStatus = TEXT("invalid_server_time_or_policy");
		}
	}
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRShadow TIMING_POLICY key=%u status=%s clientTimeValid=%d serverTimeValid=%d receiptTime=%.6f clientReportedTime=%.6f clientShotAgeMs=%.1f serverObservedRTTMs=%.1f assumedAdditionalSmoothingMs=0.0 clientBaselineTime=%.6f serverBaselineTime=%.6f serverMinusClientMs=%.1f maxRewindMs=%.1f proposedUnavailablePolicy=present_time (SHADOW ONLY; RTT approximate; NO GAMEPLAY REWIND)",
		Query.ShotSequence, ServerTimeStatus, bClientTimeValid ? 1 : 0, bServerTimeValid ? 1 : 0,
		Timing.ServerReceiptTimeSeconds, Timing.ClientEstimatedShotTimeSeconds,
		Timing.GetObservedShotAgeSeconds() * 1000.0, Timing.ServerObservedRTTSeconds * 1000.0,
		bClientTimeValid ? ClientBaselineTime : -1.0,
		bServerTimeValid ? ServerBaselineTime : -1.0,
		bClientTimeValid && bServerTimeValid ? (ServerBaselineTime - ClientBaselineTime) * 1000.0 : 0.0,
		Timing.MaxRewindSeconds * 1000.0);

	// This always-compiled query owns the existing present-time world obstruction
	// and historical capsule intersection logic. Both debug timing anchors reuse
	// the same world obstruction; C5 will invoke this identical query once its
	// independent timing and gameplay policies have been approved.
	FEnclaveHistoricalHitQuery HistoricalQuery(World, Query.Start, Query.End, Shooter);
	if (!HistoricalQuery.IsPrepared())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRShadow SERVER key=%u skipped=historical_query_unavailable (SHADOW ONLY)",
			Query.ShotSequence);
		return;
	}

	const FEnclaveHistoricalHitQueryResult ClientResult = bClientTimeValid
		? HistoricalQuery.Trace(ClientBaselineTime) : FEnclaveHistoricalHitQueryResult();
	const FEnclaveHistoricalHitQueryResult ServerResult = bServerTimeValid
		? HistoricalQuery.Trace(ServerBaselineTime) : FEnclaveHistoricalHitQueryResult();
	const FHitResult& WorldHit = HistoricalQuery.GetWorldHit();
	const double WorldBlockDistanceCm = HistoricalQuery.GetWorldBlockDistanceCm();

	// Keep the pre-existing analytic-vs-engine self-check for current collision.
	if (bLiveHit)
	{
		AEnclaveCharacter* LiveCharacter = Cast<AEnclaveCharacter>(LiveHit.GetActor());
		UCapsuleComponent* LiveCapsule = IsValid(LiveCharacter) ? LiveCharacter->GetCapsuleComponent() : nullptr;
		if (IsValid(LiveCapsule))
		{
			FEnclaveTargetCapsuleFrame LiveCapsuleFrame;
			LiveCapsuleFrame.Center = LiveCapsule->GetComponentLocation();
			LiveCapsuleFrame.Rotation = LiveCapsule->GetComponentQuat();
			LiveCapsuleFrame.RadiusCm = LiveCapsule->GetScaledCapsuleRadius();
			LiveCapsuleFrame.HalfHeightCm = LiveCapsule->GetScaledCapsuleHalfHeight();
			double AnalyticDistanceCm = 0.0;
			FVector AnalyticPoint;
			const bool bAnalyticHit = FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(
				Query.Start, Query.End, LiveCapsuleFrame, AnalyticDistanceCm, AnalyticPoint);
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(EnclaveSSRLiveCapsuleCheck), true);
			FHitResult ComponentHit;
			const bool bComponentHit = LiveCapsule->LineTraceComponent(
				ComponentHit, Query.Start, Query.End, CapsuleParams);
			const double DeltaCm = bAnalyticHit && bComponentHit
				? FMath::Abs(AnalyticDistanceCm - static_cast<double>(ComponentHit.Distance)) : -1.0;
			ENCLAVE_LOG(LogEnclaveCombat, Log,
				"SSRShadow CAPSULE_CHECK key=%u actor=%s analytic=%d component=%d analyticDistanceCm=%.2f componentDistanceCm=%.2f deltaCm=%.3f (CURRENT-TIME SELF-CHECK; SHADOW ONLY)",
				Query.ShotSequence, *GetNameSafe(LiveCharacter), bAnalyticHit ? 1 : 0,
				bComponentHit ? 1 : 0, bAnalyticHit ? AnalyticDistanceCm : -1.0,
				bComponentHit ? static_cast<double>(ComponentHit.Distance) : -1.0, DeltaCm);
		}
	}

	// The original shot-time comparison is preserved for continuity with C3 logs.
	if (bClientTimeValid)
	{
		const bool bComparable = ClientResult.Available > 0 && ClientResult.Missing == 0;
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRShadow SERVER key=%u shotAgeMs=%.1f targetHistoryTime=%.6f presentationOffsetMs=0.0 available=%d missing=%d historicalCandidate=%s historicalEntryCm=%.1f historicalPoint=%s worldBlock=%s worldBlockCm=%.1f occluded=%d shadowHit=%d liveHit=%d liveActor=%s comparable=%d sameActor=%d (UNRECONCILED CLIENT-TIME BASELINE; SHADOW ONLY; does not authorize hits)",
			Query.ShotSequence, Timing.GetObservedShotAgeSeconds() * 1000.0,
			ClientBaselineTime, ClientResult.Available, ClientResult.Missing, *GetNameSafe(ClientResult.Target.Get()),
			ClientResult.EntryDistanceCm, *ClientResult.Point.ToCompactString(),
			*GetNameSafe(WorldHit.GetActor()), WorldBlockDistanceCm, ClientResult.bOccluded ? 1 : 0,
			ClientResult.bHit ? 1 : 0, bLiveHit ? 1 : 0, *GetNameSafe(LiveHit.GetActor()),
			bComparable ? 1 : 0,
			(ClientResult.bHit && bLiveHit && LiveHit.GetActor() == ClientResult.Target.Get()) ? 1 : 0);
	}
	else
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRShadow SERVER key=%u skipped=invalid_client_baseline (server anchor evaluated separately; SHADOW ONLY)",
			Query.ShotSequence);
	}

	// C4E telemetry: a miss must not hide the presence of a moving player
	// target. Count player-owned targets in this authoritative world, separately
	// from the historical intersection (which remains entirely unchanged).
	int32 ServerPlayerTargets = 0;
	int32 MovingPlayerTargets = 0;
	double MaxPlayerTargetSpeedCmS = 0.0;
	for (TActorIterator<AEnclaveCharacter> It(World); It; ++It)
	{
		const AEnclaveCharacter* Target = *It;
		if (!IsValid(Target) || Target == Shooter || !Target->IsPlayerControlled())
		{
			continue;
		}
		++ServerPlayerTargets;
		const double SpeedCmS = Target->GetVelocity().Size2D();
		MaxPlayerTargetSpeedCmS = FMath::Max(MaxPlayerTargetSpeedCmS, SpeedCmS);
		if (SpeedCmS >= 25.0)
		{
			++MovingPlayerTargets;
		}
	}

	// For real remote-player testing, differentiate a PlayerController target from
	// a patrol/AI target. The server-derived RTT anchor is NOT authoritative yet.
	const bool bServerHasHit = bServerTimeValid && ServerResult.bHit;
	const bool bServerMatchesLive = bServerHasHit && bLiveHit && LiveHit.GetActor() == ServerResult.Target.Get();
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRShadow ANCHORS key=%u clientValid=%d clientCandidate=%s clientHit=%d serverValid=%d serverCandidate=%s serverTargetIsPlayer=%d serverHit=%d serverMatchLive=%d serverAvailable=%d serverMissing=%d serverEntryCm=%.1f worldOccluded=%d liveActor=%s serverPlayerTargets=%d movingPlayerTargets=%d maxPlayerTargetSpeedCmS=%.1f (SERVER RTT ANCHOR ZERO-SMOOTHING BASELINE; SHADOW ONLY; no gameplay effect)",
		Query.ShotSequence, bClientTimeValid ? 1 : 0, *GetNameSafe(ClientResult.Target.Get()),
		ClientResult.bHit ? 1 : 0, bServerTimeValid ? 1 : 0,
		*GetNameSafe(ServerResult.Target.Get()),
		(ServerResult.Target.IsValid() && ServerResult.Target->IsPlayerControlled()) ? 1 : 0,
		bServerHasHit ? 1 : 0, bServerMatchesLive ? 1 : 0,
		ServerResult.Available, ServerResult.Missing, ServerResult.EntryDistanceCm,
		ServerResult.bOccluded ? 1 : 0, *GetNameSafe(LiveHit.GetActor()),
		ServerPlayerTargets, MovingPlayerTargets, MaxPlayerTargetSpeedCmS);
}

#endif
