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
 * Health and MaxHealth. Phase 2 adds the clamping logic that was
 * deliberately deferred in Phase 0: PreAttributeChange guards direct
 * attribute writes (e.g. from an Initter or a non-Effect setter),
 * PostGameplayEffectExecute guards the result of an applied Gameplay Effect
 * (the Fire ability's damage effect) specifically. Both are needed - they
 * run at different points and neither substitutes for the other.
 */
UCLASS()
class ENCLAVE_API UEnclaveAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UEnclaveAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

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
