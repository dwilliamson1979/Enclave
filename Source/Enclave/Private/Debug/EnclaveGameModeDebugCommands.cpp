// Copyright Lightcarrier Studios

// Development console commands that change the combat policy
// (UEnclaveCombatPolicyComponent) on the server.
// The whole file compiles out of Test and Shipping builds.

#include "Debug/EnclaveDebug.h"   // defines ENCLAVE_INCLUDE_DEBUG_CODE; include it before the #if

#if ENCLAVE_INCLUDE_DEBUG_CODE

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Combat/Components/EnclaveCombatPolicyComponent.h"

namespace
{
	/** Parses Args[0] as a value of TEnum by its name. False if missing or unknown. */
	template <typename TEnum>
	bool ParseEnumArg(const TArray<FString>& Args, TEnum& OutValue)
	{
		if (Args.Num() < 1)
		{
			return false;
		}
		const int64 Parsed = StaticEnum<TEnum>()->GetValueByNameString(Args[0]);
		if (Parsed == INDEX_NONE)
		{
			return false;
		}
		OutValue = static_cast<TEnum>(Parsed);
		return true;
	}

	void SetSSRTimingSource(const TArray<FString>& Args, UWorld* World)
	{
		UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(World);
		if (!CombatPolicy)
		{
			UE_LOG(LogEnclaveCombat, Warning, TEXT("Enclave.Debug.SetSSRTimingSource: run this on the server."));
			return;
		}

		EEnclaveSSRTimingSource Source;
		if (!ParseEnumArg(Args, Source))
		{
			UE_LOG(LogEnclaveCombat, Warning, TEXT("Usage: Enclave.Debug.SetSSRTimingSource ServerAnchor|EchoedStamp|Hybrid"));
			return;
		}

		CombatPolicy->SetSSRTimingSource(Source);
	}

	void SetHitRegistrationMode(const TArray<FString>& Args, UWorld* World)
	{
		UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(World);
		if (!CombatPolicy)
		{
			UE_LOG(LogEnclaveCombat, Warning, TEXT("Enclave.Debug.SetHitRegistrationMode: run this on the server."));
			return;
		}

		EEnclaveWeaponHitDetectionMode HitDetectionMode;
		if (!ParseEnumArg(Args, HitDetectionMode))
		{
			UE_LOG(LogEnclaveCombat, Warning, TEXT("Usage: Enclave.Debug.SetHitRegistrationMode Capsule|SkeletalHitboxes"));
			return;
		}

		CombatPolicy->SetWeaponHitDetectionMode(HitDetectionMode);
	}

	FAutoConsoleCommandWithWorldAndArgs SetSSRTimingSourceCommand(
		TEXT("Enclave.Debug.SetSSRTimingSource"),
		TEXT("Server only. ServerAnchor|EchoedStamp|Hybrid"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetSSRTimingSource));

	FAutoConsoleCommandWithWorldAndArgs SetHitRegistrationModeCommand(
		TEXT("Enclave.Debug.SetHitRegistrationMode"),
		TEXT("Server only. Capsule|SkeletalHitboxes"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetHitRegistrationMode));
}

#endif // ENCLAVE_INCLUDE_DEBUG_CODE
