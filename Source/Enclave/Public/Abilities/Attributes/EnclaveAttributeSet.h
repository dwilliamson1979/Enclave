// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "EnclaveAttributeSet.generated.h"

/**
 * Boilerplate accessor macro used by every GAS Attribute Set. Expands to a
 * getter for the FGameplayAttribute itself (needed by Gameplay Effects) plus
 * value get/set/init helpers for the underlying float.
 *
 * This is the standard pattern (also used by Epic's Lyra sample) - copy it
 * verbatim into any future Attribute Set rather than re-deriving it.
 */
#define ENCLAVE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Core vitality attributes for GAS-backed Enclave actors.
 *
 * Health and MaxHealth are persistent replicated attributes. Damage is a
 * transient meta attribute: damage Gameplay Effects add a positive amount to
 * Damage, PostGameplayEffectExecute consumes it, resets it to zero, and then
 * subtracts that amount from Health. Keeping incoming damage separate from
 * Health gives future armor/resistance/execution calculations a clean seam
 * without changing Health's meaning or requiring negative damage magnitudes.
 */
UCLASS()
class ENCLAVE_API UEnclaveAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UEnclaveAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ENCLAVE_ATTRIBUTE_ACCESSORS(UEnclaveAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ENCLAVE_ATTRIBUTE_ACCESSORS(UEnclaveAttributeSet, MaxHealth)

	/** Transient incoming damage accumulator. Not replicated. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Health")
	FGameplayAttributeData Damage;
	ENCLAVE_ATTRIBUTE_ACCESSORS(UEnclaveAttributeSet, Damage)

protected:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);

private:
	/** Enforces intrinsic bounds for both attribute base and current values. */
	void ClampAttributeValue(const FGameplayAttribute& Attribute, float& NewValue) const;
};
