// Copyright Lightcarrier Studios

#include "Combat/EnclaveHitRegistrationSubsystem.h"

#include "Game/EnclaveGameModeBase.h"
#include "Combat/EnclaveShotIntent.h"
#include "Combat/EnclaveHistoricalCapsuleIntersection.h"
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
	DebugCompareHistoricalCapsules(Query, bCurrentHit, OutHit, Mode);
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

	// Independent present-time obstruction, identical for both proposed timing
	// anchors. Ignore characters' CURRENT capsules, which cannot obscure past poses.
	FCollisionQueryParams WorldParams(SCENE_QUERY_STAT(EnclaveSSRShadowWorld), true);
	WorldParams.AddIgnoredActor(Shooter);
	TArray<AActor*> AttachedActors;
	Shooter->GetAttachedActors(AttachedActors);
	WorldParams.AddIgnoredActors(AttachedActors);
	for (TActorIterator<AEnclaveCharacter> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			WorldParams.AddIgnoredActor(*It);
		}
	}
	FHitResult WorldHit;
	const bool bWorldBlocked = World->LineTraceSingleByChannel(WorldHit,
		Query.Start, Query.End, ECC_Weapon, WorldParams);
	const double WorldBlockDistanceCm = bWorldBlocked
		? FVector::Dist(Query.Start, WorldHit.ImpactPoint) : -1.0;

	struct FShadowCapsuleResult
	{
		AEnclaveCharacter* Target = nullptr;
		FVector Point = FVector::ZeroVector;
		double EntryDistanceCm = -1.0;
		int32 Available = 0;
		int32 Missing = 0;
		bool bOccluded = false;
		bool bHit = false;
	};

	// Executed only with the opt-in debug CVar. This traces recorded geometry
	// mathematically; no Actor/Component moves and no damage decision changes.
	auto CheckHistoryAtTime = [&](double HistoryTime) -> FShadowCapsuleResult
	{
		FShadowCapsuleResult Result;
		double NearestDistance = FVector::Dist(Query.Start, Query.End) + 1.0;
		for (TActorIterator<AEnclaveCharacter> It(World); It; ++It)
		{
			AEnclaveCharacter* Target = *It;
			if (!IsValid(Target) || Target == Shooter)
			{
				continue;
			}
			const UEnclaveTargetHistoryComponent* History = Target->FindComponentByClass<UEnclaveTargetHistoryComponent>();
			FEnclaveTargetCapsuleFrame Capsule;
			if (!IsValid(History) || !History->TryGetCapsuleAtTime(HistoryTime, Capsule))
			{
				++Result.Missing;
				continue;
			}

			++Result.Available;
			double HitDistanceCm = 0.0;
			FVector HitPoint;
			if (FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(Query.Start, Query.End,
				Capsule, HitDistanceCm, HitPoint) && HitDistanceCm < NearestDistance)
			{
				NearestDistance = HitDistanceCm;
				Result.Target = Target;
				Result.EntryDistanceCm = HitDistanceCm;
				Result.Point = HitPoint;
			}
		}
		Result.bOccluded = Result.Target && bWorldBlocked && WorldBlockDistanceCm < Result.EntryDistanceCm;
		Result.bHit = Result.Target && !Result.bOccluded;
		return Result;
	};

	const FShadowCapsuleResult ClientResult = bClientTimeValid ? CheckHistoryAtTime(ClientBaselineTime) : FShadowCapsuleResult();
	const FShadowCapsuleResult ServerResult = bServerTimeValid ? CheckHistoryAtTime(ServerBaselineTime) : FShadowCapsuleResult();

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
			ClientBaselineTime, ClientResult.Available, ClientResult.Missing, *GetNameSafe(ClientResult.Target),
			ClientResult.EntryDistanceCm, *ClientResult.Point.ToCompactString(),
			*GetNameSafe(WorldHit.GetActor()), WorldBlockDistanceCm, ClientResult.bOccluded ? 1 : 0,
			ClientResult.bHit ? 1 : 0, bLiveHit ? 1 : 0, *GetNameSafe(LiveHit.GetActor()),
			bComparable ? 1 : 0,
			(ClientResult.bHit && bLiveHit && LiveHit.GetActor() == ClientResult.Target) ? 1 : 0);
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
	const bool bServerMatchesLive = bServerHasHit && bLiveHit && LiveHit.GetActor() == ServerResult.Target;
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRShadow ANCHORS key=%u clientValid=%d clientCandidate=%s clientHit=%d serverValid=%d serverCandidate=%s serverTargetIsPlayer=%d serverHit=%d serverMatchLive=%d serverAvailable=%d serverMissing=%d serverEntryCm=%.1f worldOccluded=%d liveActor=%s serverPlayerTargets=%d movingPlayerTargets=%d maxPlayerTargetSpeedCmS=%.1f (SERVER RTT ANCHOR ZERO-SMOOTHING BASELINE; SHADOW ONLY; no gameplay effect)",
		Query.ShotSequence, bClientTimeValid ? 1 : 0, *GetNameSafe(ClientResult.Target),
		ClientResult.bHit ? 1 : 0, bServerTimeValid ? 1 : 0,
		*GetNameSafe(ServerResult.Target),
		(IsValid(ServerResult.Target) && ServerResult.Target->IsPlayerControlled()) ? 1 : 0,
		bServerHasHit ? 1 : 0, bServerMatchesLive ? 1 : 0,
		ServerResult.Available, ServerResult.Missing, ServerResult.EntryDistanceCm,
		ServerResult.bOccluded ? 1 : 0, *GetNameSafe(LiveHit.GetActor()),
		ServerPlayerTargets, MovingPlayerTargets, MaxPlayerTargetSpeedCmS);
}

#endif
