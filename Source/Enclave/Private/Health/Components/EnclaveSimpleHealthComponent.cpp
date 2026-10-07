// Copyright Lightcarrier Studios

#include "Health/Components/EnclaveSimpleHealthComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#include "Combat/EnclaveDamageRequest.h"

UEnclaveSimpleHealthComponent::UEnclaveSimpleHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEnclaveSimpleHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	MaxHealth = FMath::Max(0.0f, MaxHealth);
	Health = MaxHealth;
}

bool UEnclaveSimpleHealthComponent::ReceiveDamage(const FEnclaveDamageRequest& DamageRequest)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || DamageRequest.Damage <= 0.0f || IsHealthDepleted())
	{
		return false;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - DamageRequest.Damage, 0.0f, MaxHealth);

	NotifyHealthChanged(OldHealth, Health);
	return !FMath::IsNearlyEqual(OldHealth, Health);
}

void UEnclaveSimpleHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveSimpleHealthComponent, Health);
	DOREPLIFETIME(UEnclaveSimpleHealthComponent, MaxHealth);
}

void UEnclaveSimpleHealthComponent::OnRep_Health(float OldHealth)
{
	NotifyHealthChanged(OldHealth, Health);
}

void UEnclaveSimpleHealthComponent::OnRep_MaxHealth(float OldMaxHealth)
{
	NotifyMaxHealthChanged(OldMaxHealth, MaxHealth);
}
