// Copyright Lightcarrier Studios

#include "Health/Components/EnclaveHealthComponent.h"

#include "GameFramework/Actor.h"

UEnclaveHealthComponent::UEnclaveHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UEnclaveHealthComponent* UEnclaveHealthComponent::FindHealthComponent(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UEnclaveHealthComponent>() : nullptr;
}

float UEnclaveHealthComponent::GetHealth() const
{
	return GetHealthValue();
}

float UEnclaveHealthComponent::GetMaxHealth() const
{
	return GetMaxHealthValue();
}

float UEnclaveHealthComponent::GetHealthNormalized() const
{
	const float MaxHealth = GetMaxHealth();
	return MaxHealth > 0.0f ? FMath::Clamp(GetHealth() / MaxHealth, 0.0f, 1.0f) : 0.0f;
}

bool UEnclaveHealthComponent::HasHealthRemaining() const
{
	return GetHealth() > 0.0f;
}

bool UEnclaveHealthComponent::IsHealthDepleted() const
{
	return !HasHealthRemaining();
}

void UEnclaveHealthComponent::NotifyHealthChanged(float OldHealth, float NewHealth)
{
	if (FMath::IsNearlyEqual(OldHealth, NewHealth))
	{
		return;
	}

	OnHealthChanged.Broadcast(this, OldHealth, NewHealth);

	if (OldHealth > 0.0f && NewHealth <= 0.0f)
	{
		OnHealthDepleted.Broadcast(this);
	}
}

void UEnclaveHealthComponent::NotifyMaxHealthChanged(float OldMaxHealth, float NewMaxHealth)
{
	if (!FMath::IsNearlyEqual(OldMaxHealth, NewMaxHealth))
	{
		OnMaxHealthChanged.Broadcast(this, OldMaxHealth, NewMaxHealth);
	}
}
