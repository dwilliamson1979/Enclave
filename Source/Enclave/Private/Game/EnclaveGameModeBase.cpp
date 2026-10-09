// Copyright Lightcarrier Studios

#include "Game/EnclaveGameModeBase.h"

#include "Kismet/GameplayStatics.h"

#include "Debug/EnclaveDebug.h"
#include "Player/EnclavePlayerState.h"
#include "Characters/EnclaveCharacter.h"
#include "EngineUtils.h"

AEnclaveGameModeBase::AEnclaveGameModeBase()
{
	PlayerStateClass = AEnclavePlayerState::StaticClass();
}

void AEnclaveGameModeBase::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

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
	for (TActorIterator<AEnclaveCharacter> It(GetWorld()); It; ++It)
	{
		It->ApplyWeaponHitDetectionCollisionPolicy();
	}
	UE_LOG(LogEnclaveCombat, Log, TEXT("Active hit registration mode: %s"),
		GetEnclaveWeaponHitDetectionModeName(WeaponHitDetectionMode));

	// FUTURE: replicate selected policy via GameState for HUD / spectators.
	// FUTURE: invalidate/reseed rewind history when switching geometry.
	// Production matches should lock this policy after match start.
}

void AEnclaveGameModeBase::EnclaveSetHitRegistrationMode(const FString& ModeName)
{
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
}
