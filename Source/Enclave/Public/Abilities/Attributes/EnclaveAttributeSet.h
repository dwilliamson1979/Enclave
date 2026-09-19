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
 * Phase 0 skeleton Attribute Set: Health and MaxHealth only.
 *
 * Deliberately does not yet clamp Health or react to Gameplay Effects
 * (PreAttributeChange / PostGameplayEffectExecute) - that logic belongs with
 * the damage-application work in a later phase, once there's an actual
 * source of damage to test it against. For now this only needs to exist,
 * replicate, and be visible/changeable for the Phase 0 checkpoint.
 */
UCLASS()
class ENCLAVE_API UEnclaveAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UEnclaveAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes|Health")
	FGameplayAttributeData Health;
	ENCLAVE_ATTRIBUTE_ACCESSORS(UEnclaveAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes|Health")
	FGameplayAttributeData MaxHealth;
	ENCLAVE_ATTRIBUTE_ACCESSORS(UEnclaveAttributeSet, MaxHealth)

protected:
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
};
