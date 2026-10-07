// Copyright Lightcarrier Studios

#include "Health/Components/EnclaveGASHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"

#include "Abilities/Attributes/EnclaveAttributeSet.h"
#include "Abilities/Effects/EnclaveGE_Damage.h"
#include "Combat/EnclaveDamageRequest.h"
#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveGASHealthComponent::UEnclaveGASHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveGASHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent == InAbilitySystemComponent && HealthChangedDelegateHandle.IsValid() && MaxHealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	UninitializeFromAbilitySystem();

	if (!IsValid(InAbilitySystemComponent) || !InAbilitySystemComponent->GetSet<UEnclaveAttributeSet>())
	{
		return;
	}

	AbilitySystemComponent = InAbilitySystemComponent;

	HealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UEnclaveGASHealthComponent::HandleHealthChanged);

	MaxHealthChangedDelegateHandle = AbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UEnclaveGASHealthComponent::HandleMaxHealthChanged);
}

void UEnclaveGASHealthComponent::UninitializeFromAbilitySystem()
{
	if (IsValid(AbilitySystemComponent))
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
		}

		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			AbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedDelegateHandle);
		}
	}

	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
	AbilitySystemComponent = nullptr;
}

bool UEnclaveGASHealthComponent::ReceiveDamage(const FEnclaveDamageRequest& DamageRequest)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(AbilitySystemComponent) || DamageRequest.Damage <= 0.0f || IsHealthDepleted())
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageRequest.SourceActor.Get());
	UAbilitySystemComponent* EffectSourceASC = IsValid(SourceASC) ? SourceASC : AbilitySystemComponent.Get();

	FGameplayEffectContextHandle ContextHandle = EffectSourceASC->MakeEffectContext();

	if (IsValid(DamageRequest.DamageCauser))
	{
		ContextHandle.AddSourceObject(DamageRequest.DamageCauser.Get());
	}

	if (DamageRequest.HitResult.bBlockingHit)
	{
		ContextHandle.AddHitResult(DamageRequest.HitResult);
	}

	FGameplayEffectSpecHandle SpecHandle = EffectSourceASC->MakeOutgoingSpec(UEnclaveGE_Damage::StaticClass(), 1.0f, ContextHandle);
	if (!SpecHandle.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(EnclaveGameplayTags::Data_Damage, DamageRequest.Damage);

	const float OldHealth = GetHealth();

	if (EffectSourceASC == AbilitySystemComponent)
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	else
	{
		EffectSourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), AbilitySystemComponent);
	}

	return GetHealth() < OldHealth;
}

void UEnclaveGASHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

float UEnclaveGASHealthComponent::GetHealthValue() const
{
	return IsValid(AbilitySystemComponent)
		? AbilitySystemComponent->GetNumericAttribute(UEnclaveAttributeSet::GetHealthAttribute())
		: 0.0f;
}

float UEnclaveGASHealthComponent::GetMaxHealthValue() const
{
	return IsValid(AbilitySystemComponent)
		? AbilitySystemComponent->GetNumericAttribute(UEnclaveAttributeSet::GetMaxHealthAttribute())
		: 0.0f;
}

void UEnclaveGASHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	NotifyHealthChanged(ChangeData.OldValue, ChangeData.NewValue);
}

void UEnclaveGASHealthComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	NotifyMaxHealthChanged(ChangeData.OldValue, ChangeData.NewValue);
}
