// Copyright Lightcarrier Studios

#include "Debug/EnclaveGASDebugBlueprintLibrary.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"

#include "Debug/EnclaveDebug.h"

float UEnclaveGASDebugBlueprintLibrary::GetAttributeValue(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bFound)
{
	bFound = false;

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsValid(AbilitySystemComponent) || !Attribute.IsValid() || !AbilitySystemComponent->HasAttributeSetForAttribute(Attribute))
	{
		return 0.0f;
	}

	bFound = true;
	return AbilitySystemComponent->GetNumericAttribute(Attribute);
#else
	return 0.0f;
#endif
}

float UEnclaveGASDebugBlueprintLibrary::GetAttributeBaseValue(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, bool& bFound)
{
	bFound = false;

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsValid(AbilitySystemComponent) || !Attribute.IsValid() || !AbilitySystemComponent->HasAttributeSetForAttribute(Attribute))
	{
		return 0.0f;
	}

	bFound = true;
	return AbilitySystemComponent->GetNumericAttributeBase(Attribute);
#else
	return 0.0f;
#endif
}

bool UEnclaveGASDebugBlueprintLibrary::SetAttributeBaseValue(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute Attribute, float NewBaseValue)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsValid(AbilitySystemComponent) || !Attribute.IsValid() || !AbilitySystemComponent->HasAttributeSetForAttribute(Attribute))
	{
		return false;
	}

	const AActor* OwnerActor = AbilitySystemComponent->GetOwnerActor();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	AbilitySystemComponent->SetNumericAttributeBase(Attribute, NewBaseValue);
	return true;
#else
	return false;
#endif
}

int32 UEnclaveGASDebugBlueprintLibrary::RemoveActiveEffectsWithTags(UAbilitySystemComponent* AbilitySystemComponent, FGameplayTagContainer EffectTags)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsValid(AbilitySystemComponent) || EffectTags.IsEmpty())
	{
		return 0;
	}

	const AActor* OwnerActor = AbilitySystemComponent->GetOwnerActor();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return 0;
	}

	return AbilitySystemComponent->RemoveActiveEffectsWithTags(EffectTags);
#else
	return 0;
#endif
}
