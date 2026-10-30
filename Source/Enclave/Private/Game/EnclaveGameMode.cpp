// Copyright Lightcarrier Studios

#include "Game/EnclaveGameMode.h"

#include "Kismet/GameplayStatics.h"

#include "Debug/EnclaveDebug.h"
#include "Player/EnclavePlayerState.h"
#include "Game/EnclaveGameState.h"
#include "UI/EnclaveHUD.h"

AEnclaveGameMode::AEnclaveGameMode()
{
	PlayerStateClass = AEnclavePlayerState::StaticClass();
	GameStateClass = AEnclaveGameState::StaticClass();
	HUDClass = AEnclaveHUD::StaticClass();
}

void AEnclaveGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MaxRewindAllowanceMs = FMath::Clamp(MaxRewindAllowanceMs, 0.0f, 500.0f);
	SSRAnchorOffsetMs = FMath::Clamp(SSRAnchorOffsetMs, -150.0f, 100.0f);
	SSRStampEarlyToleranceMs = FMath::Clamp(SSRStampEarlyToleranceMs, 0.0f, 100.0f);
	SSRStampLateToleranceMs = FMath::Clamp(SSRStampLateToleranceMs, 0.0f, 100.0f);
	ParseEnumOption(Options, TEXT("LagCompensation"), LagCompensationMode);
	ParseEnumOption(Options, TEXT("SSRTimingSource"), SSRTimingSource);

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

	UE_LOG(LogEnclaveCombat, Log,
		TEXT("Match lag compensation: %s (timing=%s anchorOffset=%.1f ms stampWindow=-%.1f/+%.1f ms rewindAllowance=%.1f ms unavailable=%s)."),
		*UEnum::GetValueAsString(LagCompensationMode), *UEnum::GetValueAsString(SSRTimingSource),
		SSRAnchorOffsetMs, SSRStampEarlyToleranceMs, SSRStampLateToleranceMs, MaxRewindAllowanceMs,
		*UEnum::GetValueAsString(SSRUnavailableFallbackPolicy));
	if (LagCompensationMode == EEnclaveLagCompensationMode::ServerSideRewind && !IsServerSideRewindActive())
	{
		UE_LOG(LogEnclaveCombat, Warning,
			TEXT("Server-Side Rewind supports Capsule hit detection only; %s shots stay present-time."),
			GetEnclaveWeaponHitDetectionModeName(WeaponHitDetectionMode));
	}
}

template <typename TEnum>
void AEnclaveGameMode::ParseEnumOption(const FString& Options, const TCHAR* Key, TEnum& InOutValue)
{
	const FString Value = UGameplayStatics::ParseOption(Options, Key);
	if (Value.IsEmpty())
	{
		return;
	}
	const int64 Parsed = StaticEnum<TEnum>()->GetValueByNameString(Value);
	if (Parsed == INDEX_NONE)
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("Ignoring invalid %s server option '%s'."), Key, *Value);
		return;
	}
	InOutValue = static_cast<TEnum>(Parsed);
}

FEnclaveSSRTimingSettings AEnclaveGameMode::GetSSRTimingSettings() const
{
	FEnclaveSSRTimingSettings Settings;
	Settings.Source = SSRTimingSource;
	Settings.AnchorOffsetSeconds = SSRAnchorOffsetMs / 1000.0;
	Settings.StampEarlyToleranceSeconds = SSRStampEarlyToleranceMs / 1000.0;
	Settings.StampLateToleranceSeconds = SSRStampLateToleranceMs / 1000.0;
	return Settings;
}

void AEnclaveGameMode::SetSSRTimingSource(EEnclaveSSRTimingSource TimingSource)
{
	if (!HasAuthority()) { return; }
	if (SSRTimingSource == TimingSource) { return; }

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	if (HasMatchStarted())
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("Cannot change SSR Timing Source after match start in production builds."));
		return;
	}
#endif

	SSRTimingSource = TimingSource;
	UE_LOG(LogEnclaveCombat, Log, TEXT("Active SSR timing source: %s"), *UEnum::GetValueAsString(SSRTimingSource));
}

void AEnclaveGameMode::SetWeaponHitDetectionMode(EEnclaveWeaponHitDetectionMode NewMode)
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
