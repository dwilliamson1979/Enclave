// Copyright Lightcarrier Studios
#include "Abilities/EnclaveAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"

namespace
{
FPredictionKey GetActiveInstancePredictionKey(const FGameplayAbilitySpec& Spec)
{
	const UGameplayAbility* Instance = Spec.GetPrimaryInstance();
	return Instance ? Instance->GetCurrentActivationInfo().GetActivationPredictionKey() : FPredictionKey();
}
}

bool UEnclaveAbilitySystemComponent::PressAbilityInput(FGameplayAbilitySpecHandle Handle, bool bRoutesReleaseEvents)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		return false;
	}

	// Started-only actions (e.g. Reload) do not have a release edge.
	// Do not latch InputPressed or create a synthetic release for them.
	if (!bRoutesReleaseEvents)
	{
		return Spec->IsActive() || TryActivateAbility(Handle);
	}

	if (!Spec->InputPressed)
	{
		Spec->InputPressed = true;
		if (Spec->IsActive())
		{
			AbilitySpecInputPressed(*Spec);
		}
	}

	// A press may target an ability that is already active; don't restart it.
	return Spec->IsActive() || TryActivateAbility(Handle);
}

void UEnclaveAbilitySystemComponent::ReleaseAbilityInput(FGameplayAbilitySpecHandle Handle)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec || !Spec->InputPressed)
	{
		return; // Spec was revoked, or the release was already consumed.
	}

	Spec->InputPressed = false;
	if (Spec->IsActive())
	{
		AbilitySpecInputReleased(*Spec);
	}
}

void UEnclaveAbilitySystemComponent::AbilitySpecInputPressed(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputPressed(Spec);
	if (Spec.IsActive())
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
			GetActiveInstancePredictionKey(Spec));
	}
}

void UEnclaveAbilitySystemComponent::AbilitySpecInputReleased(FGameplayAbilitySpec& Spec)
{
	Super::AbilitySpecInputReleased(Spec);
	if (Spec.IsActive())
	{
		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
			GetActiveInstancePredictionKey(Spec));
	}
}
