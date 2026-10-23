// Copyright Lightcarrier Studios

#include "Combat/EnclaveHitRegistrationSubsystem.h"

#include "Game/EnclaveGameModeBase.h"
#include "Combat/EnclaveShotIntent.h"
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
	const double DeltaSeconds = GameState->GetServerWorldTimeSeconds() - Intent.ClientServerTimeSeconds;
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

	return TraceCurrentGeometry(Query, OutHit, Mode);
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
