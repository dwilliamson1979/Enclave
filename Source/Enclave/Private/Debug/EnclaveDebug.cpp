// Copyright Lightcarrier Studios

#include "Debug/EnclaveDebug.h"

#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogEnclaveCharacter);
DEFINE_LOG_CATEGORY(LogEnclaveInput);
DEFINE_LOG_CATEGORY(LogEnclaveAbilities);
DEFINE_LOG_CATEGORY(LogEnclaveGameplayCue);
DEFINE_LOG_CATEGORY(LogEnclaveWeapon);
DEFINE_LOG_CATEGORY(LogEnclaveEquipment);
DEFINE_LOG_CATEGORY(LogEnclaveInventory);
DEFINE_LOG_CATEGORY(LogEnclaveCombat);
DEFINE_LOG_CATEGORY(LogEnclaveReplication);

namespace EnclaveDebug
{
	FString GetNetModeString(const AActor* Actor)
	{
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (!Actor)
		{
			return TEXT("NoActor");
		}

		switch (Actor->GetNetMode())
		{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
		}
#else
		return FString();
#endif
	}

	FString GetNetRoleString(const AActor* Actor)
	{
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (!Actor)
		{
			return TEXT("NoActor");
		}

		const UEnum* RoleEnum = StaticEnum<ENetRole>();

		const FString LocalRoleString = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(Actor->GetLocalRole())) : TEXT("Unknown");
		const FString RemoteRoleString = RoleEnum ? RoleEnum->GetNameStringByValue(static_cast<int64>(Actor->GetRemoteRole())) : TEXT("Unknown");

		return FString::Printf(TEXT("LocalRole: %s, RemoteRole: %s"), *LocalRoleString, *RemoteRoleString);
#else
		return FString();
#endif
	}

	FString BuildNetworkContextString(const AActor* Actor, const FString& Message, bool bIncludeNetMode, bool bIncludeNetRole)
	{
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (!Actor)
		{
			return FString::Printf(TEXT("(NoActor) %s"), *Message);
		}

		FString FinalMessage;

		if (bIncludeNetMode)
		{
			FinalMessage += FString::Printf(TEXT("(NetMode: %s) "), *GetNetModeString(Actor));
		}

		if (bIncludeNetRole)
		{
			FinalMessage += FString::Printf(TEXT("(NetRole: %s) "), *GetNetRoleString(Actor));
		}

		FinalMessage += FString::Printf(TEXT("(%s) %s"), *GetNameSafe(Actor), *Message);
		return FinalMessage;
#else
		return FString();
#endif
	}

	void PrintScreen(const FString& Message, FColor Color, float Duration)
	{
#if ENCLAVE_INCLUDE_DEBUG_CODE
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, Duration, Color, Message);
		}
#endif
	}

	void PrintNetworkContext(const AActor* Actor, const FString& Message, FColor Color, float Duration, bool bIncludeNetMode, bool bIncludeNetRole)
	{
#if ENCLAVE_INCLUDE_DEBUG_CODE
		PrintScreen(BuildNetworkContextString(Actor, Message, bIncludeNetMode, bIncludeNetRole), Color, Duration);
#endif
	}
}
