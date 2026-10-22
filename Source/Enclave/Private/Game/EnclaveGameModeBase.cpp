// Copyright Lightcarrier Studios

#include "Game/EnclaveGameModeBase.h"

#include "Kismet/GameplayStatics.h"

#include "Debug/EnclaveDebug.h"
#include "Player/EnclavePlayerState.h"

AEnclaveGameModeBase::AEnclaveGameModeBase()
{
	PlayerStateClass = AEnclavePlayerState::StaticClass();
}

void AEnclaveGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

    // Editor-exposed SSR configuration is only a contract at this milestone.
    // Never claim rewind is active when RegisterHitscan still uses current geometry.
    MaxRewindAllowanceMs = FMath::Clamp(MaxRewindAllowanceMs, 0.0f, 500.0f);
    if (LagCompensationMode != EEnclaveLagCompensationMode::Disabled)
    {
        UE_LOG(LogEnclaveCombat, Warning,
            TEXT("Server-Side Rewind is not implemented; forcing lag compensation to Disabled (present-time traces)."));
        LagCompensationMode = EEnclaveLagCompensationMode::Disabled;
    }
    UE_LOG(LogEnclaveCombat, Log, TEXT("Match lag compensation: Disabled; future rewind allowance: %.1f ms."), MaxRewindAllowanceMs);

	const FString HitDetectionModeOption = UGameplayStatics::ParseOption(Options, TEXT("WeaponHitDetectionMode"));

	if (!HitDetectionModeOption.IsEmpty())
	{
		if (HitDetectionModeOption.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase))
		{
			WeaponHitDetectionMode = EEnclaveWeaponHitDetectionMode::Capsule;
		}
		else if (HitDetectionModeOption.Equals(TEXT("SkeletalHitboxes"), ESearchCase::IgnoreCase))
		{
			WeaponHitDetectionMode = EEnclaveWeaponHitDetectionMode::SkeletalHitboxes;
		}
		else
		{
			UE_LOG(
				LogEnclaveCombat,
				Warning,
				TEXT("Ignoring invalid WeaponHitDetectionMode server option '%s'. Expected Capsule or SkeletalHitboxes."),
				*HitDetectionModeOption);
		}
	}

	UE_LOG(
		LogEnclaveCombat,
		Log,
		TEXT("Match weapon hit detection mode: %s."),
		GetEnclaveWeaponHitDetectionModeName(WeaponHitDetectionMode));
}

void AEnclaveGameModeBase::SetWeaponHitDetectionMode(EEnclaveWeaponHitDetectionMode NewMode)
{
	if (!HasAuthority()) { return; }

	// Production policy is immutable after match start. Development builds may
	// deliberately switch modes mid-match to test live collision transitions.
	// Keep this guard in the setter: Blueprint callers must obey it too.
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	if (HasMatchStarted())
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("Cannot change hit registration mode after match start in production builds."));
		return;
	}
#endif
	if (NewMode != EEnclaveWeaponHitDetectionMode::Capsule &&
		NewMode != EEnclaveWeaponHitDetectionMode::SkeletalHitboxes)
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("Rejected unsupported hit registration mode."));
		return;
	}
	if (WeaponHitDetectionMode == NewMode) { return; }

	WeaponHitDetectionMode = NewMode;
	// On the game thread, finish collision-policy transition before the next
	// authoritative shot trace. Existing pawns must transition as well as new ones.
	WeaponHitDetectionModeChanged.Broadcast(WeaponHitDetectionMode);
	UE_LOG(LogEnclaveCombat, Log, TEXT("Active hit registration mode: %s"),
		GetEnclaveWeaponHitDetectionModeName(WeaponHitDetectionMode));

	// FUTURE: replicate selected policy via GameState for HUD / spectators.
	// FUTURE: invalidate/reseed rewind history when switching geometry.
	// Shipping/Test builds lock this policy after match start.
}

void AEnclaveGameModeBase::EnclaveSetHitRegistrationMode(const FString& ModeName)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (ModeName.Equals(TEXT("Capsule"), ESearchCase::IgnoreCase))
	{
		SetWeaponHitDetectionMode(EEnclaveWeaponHitDetectionMode::Capsule);
	}
	else if (ModeName.Equals(TEXT("SkeletalHitboxes"), ESearchCase::IgnoreCase))
	{
		SetWeaponHitDetectionMode(EEnclaveWeaponHitDetectionMode::SkeletalHitboxes);
	}
	else
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("Usage: EnclaveSetHitRegistrationMode Capsule|SkeletalHitboxes"));
	}
#else
	// The reflected Exec function remains declared for UHT compatibility, but cannot change match rules.
	(void)ModeName;
#endif
}
