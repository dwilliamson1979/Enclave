// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "EnclaveGASDebugBlueprintLibrary.generated.h"

class UAbilitySystemComponent;

/**
 * Development-only Blueprint conveniences for probing GAS state.
 *
 * These helpers are intentionally not gameplay APIs. Mutating operations are
 * authority-only and all functions become inert in Test/Shipping builds.
 */
UCLASS()
class ENCLAVE_API UEnclaveGASDebugBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the current/final value of an attribute after active modifiers. */
	UFUNCTION(BlueprintPure, Category = "Enclave|Debug|GAS", meta = (DisplayName = "Debug Get GAS Attribute Value"))
	static float GetAttributeValue(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bFound);

	/** Returns the attribute base value before stateful modifiers. */
	UFUNCTION(BlueprintPure, Category = "Enclave|Debug|GAS", meta = (DisplayName = "Debug Get GAS Attribute Base Value"))
	static float GetAttributeBaseValue(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bFound);

	/**
	 * Sets an attribute base value through GAS's aggregator-aware API.
	 * Intended only for quick development tests.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enclave|Debug|GAS", meta = (DisplayName = "Debug Set GAS Attribute Base Value"))
	static bool SetAttributeBaseValue(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, float NewBaseValue);

	/** Removes active Gameplay Effects matching any supplied effect/asset tag. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Enclave|Debug|GAS", meta = (DisplayName = "Debug Remove GAS Effects With Tags"))
	static int32 RemoveActiveEffectsWithTags(UAbilitySystemComponent* AbilitySystemComponent, FGameplayTagContainer EffectTags);
};
