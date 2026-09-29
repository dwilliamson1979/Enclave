// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"

class AActor;

/**
 * Enclave debug logging and on-screen printing.
 *
 * Everything routed through the ENCLAVE_* macros below is compiled out of
 * Test and Shipping builds - the call, its arguments, and any string
 * formatting all disappear. Use the macros for trace/diagnostic output.
 *
 * For a real problem you still want in a Test build's log (misconfigured
 * data, an unhandled case that should never happen), call UE_LOG directly
 * with one of the Enclave categories instead of ENCLAVE_LOG.
 *
 * Runtime toggles stay with the existing Enclave.Debug.* CVars; these macros
 * go inside those CVar checks.
 */

#define ENCLAVE_INCLUDE_DEBUG_CODE (!UE_BUILD_SHIPPING && !UE_BUILD_TEST)
#define ENCLAVE_DRAW_DEBUG (ENCLAVE_INCLUDE_DEBUG_CODE && ENABLE_DRAW_DEBUG)

// Variadic so a code block containing unparenthesized commas (template
// arguments, braced initializers) still works as a single argument.
#if ENCLAVE_INCLUDE_DEBUG_CODE
	#define ENCLAVE_DEBUG_CODE(...) do { __VA_ARGS__ } while (0)
#else
	#define ENCLAVE_DEBUG_CODE(...) do { } while (0)
#endif

// Log categories, one per gameplay system, so the Output Log can be filtered
// (and verbosity raised per system, e.g. `Log LogEnclaveWeapon Verbose`).
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveCharacter, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveAvatarPresentation, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveInput, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveAbilities, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveGameplayCue, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveWeapon, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveEquipment, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveInventory, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveCombat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEnclaveReplication, Log, All);

// ENCLAVE_LOG(LogEnclaveWeapon, Log, "Ammo=%d Owner=%s", Ammo, *GetNameSafe(GetOwner()));
// Format must be a string literal (it is wrapped in TEXT()).
#define ENCLAVE_LOG(Category, Verbosity, Format, ...) \
	ENCLAVE_DEBUG_CODE(UE_LOG(Category, Verbosity, TEXT(Format), ##__VA_ARGS__);)

// ENCLAVE_LOG_MSG(LogEnclaveWeapon, Log, SomeFString);
#define ENCLAVE_LOG_MSG(Category, Verbosity, Message) \
	ENCLAVE_DEBUG_CODE(UE_LOG(Category, Verbosity, TEXT("%s"), *FString(Message));)

// ENCLAVE_SCREEN(FString::Printf(TEXT("Ammo=%d"), Ammo), FColor::Yellow, 3.0f);
#define ENCLAVE_SCREEN(Message, Color, Duration) \
	ENCLAVE_DEBUG_CODE(EnclaveDebug::PrintScreen(Message, Color, Duration);)

// ENCLAVE_SCREEN_DEFAULT(TEXT("Hello"));  - green, 2 seconds
#define ENCLAVE_SCREEN_DEFAULT(Message) \
	ENCLAVE_DEBUG_CODE(EnclaveDebug::PrintScreen(Message);)

// Prefixes the message with the actor's net mode, net role and name:
// "(NetMode: Client) (NetRole: LocalRole: ROLE_AutonomousProxy, RemoteRole: ROLE_Authority) (BP_Char_C_0) msg"
#define ENCLAVE_NET_SCREEN(Actor, Message, Color, Duration) \
	ENCLAVE_DEBUG_CODE(EnclaveDebug::PrintNetworkContext(Actor, Message, Color, Duration);)

#define ENCLAVE_NET_LOG(Category, Verbosity, Actor, Message) \
	ENCLAVE_DEBUG_CODE(UE_LOG(Category, Verbosity, TEXT("%s"), *EnclaveDebug::BuildNetworkContextString(Actor, Message));)

/**
 * Helpers behind the macros. Defined in all build configurations (they
 * return empty strings / do nothing outside debug builds), so a direct call
 * never causes a link error - but prefer the macros, which also remove the
 * argument evaluation.
 */
namespace EnclaveDebug
{
	/** "Standalone", "DedicatedServer", "ListenServer", "Client". */
	ENCLAVE_API FString GetNetModeString(const AActor* Actor);

	/** "LocalRole: ROLE_x, RemoteRole: ROLE_y". */
	ENCLAVE_API FString GetNetRoleString(const AActor* Actor);

	ENCLAVE_API FString BuildNetworkContextString(const AActor* Actor, const FString& Message, bool bIncludeNetMode = true, bool bIncludeNetRole = true);

	ENCLAVE_API void PrintScreen(const FString& Message, FColor Color = FColor::Green, float Duration = 2.0f);

	ENCLAVE_API void PrintNetworkContext(const AActor* Actor, const FString& Message, FColor Color = FColor::Green, float Duration = 2.0f, bool bIncludeNetMode = true, bool bIncludeNetRole = true);
}
