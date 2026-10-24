// Copyright Lightcarrier Studios

#include "Health/Components/EnclaveGASHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"

#include "Abilities/Attributes/EnclaveAttributeSet.h"
#include "Abilities/Effects/EnclaveGE_Damage.h"
#include "Combat/EnclaveDamageRequest.h"
#include "GameplayTags/EnclaveAbilityTags.h"
#include "Debug/EnclaveDebug.h"
#if ENCLAVE_INCLUDE_DEBUG_CODE
#include "Characters/EnclaveCharacter.h"
#endif

UEnclaveGASHealthComponent::UEnclaveGASHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveGASHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent.Get() == InAbilitySystemComponent && HealthChangedDelegateHandle.IsValid() && MaxHealthChangedDelegateHandle.IsValid())
	{
		return;
	}

	UninitializeFromAbilitySystem();

	if (!IsValid(InAbilitySystemComponent))
	{
		return;
	}

	const UEnclaveAttributeSet* AttributeSet = InAbilitySystemComponent->GetSet<UEnclaveAttributeSet>();
	if (!IsValid(AttributeSet))
	{
		return;
	}

	AbilitySystemComponent = InAbilitySystemComponent;

	HealthChangedDelegateHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UEnclaveGASHealthComponent::HandleHealthChanged);

	MaxHealthChangedDelegateHandle = InAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UEnclaveGASHealthComponent::HandleMaxHealthChanged);

	InitializeAvatarHealthIfNeeded(InAbilitySystemComponent, AttributeSet);
}

void UEnclaveGASHealthComponent::InitializeAvatarHealthIfNeeded(UAbilitySystemComponent* InAbilitySystemComponent, const UEnclaveAttributeSet* AttributeSet)
{
	AActor* Owner = GetOwner();

	if (bHasInitializedAvatarHealth || !IsValid(Owner) || !Owner->HasAuthority() || !IsValid(InAbilitySystemComponent) || !IsValid(AttributeSet))
	{
		return;
	}

	// The ASC/AttributeSet live on PlayerState and survive Pawn replacement, so
	// their Health value also survives unless a new avatar explicitly initializes
	// it. This component instance is avatar-scoped: initialize exactly once for
	// its lifetime, using the ASC's current MaxHealth as the new avatar's health.
	bHasInitializedAvatarHealth = true;
	InAbilitySystemComponent->SetNumericAttributeBase(UEnclaveAttributeSet::GetHealthAttribute(), AttributeSet->GetMaxHealth());
}

void UEnclaveGASHealthComponent::UninitializeFromAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = AbilitySystemComponent.Get())
	{
		if (HealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedDelegateHandle);
		}

		if (MaxHealthChangedDelegateHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UEnclaveAttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedDelegateHandle);
		}
	}

	HealthChangedDelegateHandle.Reset();
	MaxHealthChangedDelegateHandle.Reset();
	AbilitySystemComponent.Reset();
}

bool UEnclaveGASHealthComponent::ReceiveDamage(const FEnclaveDamageRequest& DamageRequest)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* TargetASC = AbilitySystemComponent.Get();

	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(TargetASC) || DamageRequest.Damage <= 0.0f || IsHealthDepleted())
	{
		return false;
	}

#if ENCLAVE_INCLUDE_DEBUG_CODE
	// D1.1B long-duration SSR sampling: the local-authority strafing character
	// must stay alive. Suppress only this explicitly marked body's health mutation;
	// preserve the accepted-hit result for the ordinary GA_Fire path. This never
	// compiles into Test/Shipping and never grants SSR hit authority.
	if (const AEnclaveCharacter* TestTarget = Cast<AEnclaveCharacter>(Owner))
	{
		if (TestTarget->DebugIsSSRStrafeDamageSuppressed())
		{
			return true;
		}
	}
#endif

	// Source is operation context, not a lifecycle relationship. Resolve it per
	// damage request rather than caching an attacker that can change every hit.
	UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageRequest.SourceActor.Get());
	UAbilitySystemComponent* EffectSourceASC = IsValid(SourceASC) ? SourceASC : TargetASC;

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
	SpecHandle.Data->AppendDynamicAssetTags(DamageRequest.DamageTags);

	const float OldHealth = GetHealth();

	// Attribute-change delegates execute synchronously while the Gameplay Effect
	// is being applied. Keep the operation context on a stack so depletion can
	// preserve the exact request that caused it, including nested damage.
	ActiveDamageRequests.Add(DamageRequest);

	if (EffectSourceASC == TargetASC)
	{
		TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	else
	{
		EffectSourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
	}

	ActiveDamageRequests.Pop(EAllowShrinking::No);

	return GetHealth() < OldHealth;
}

void UEnclaveGASHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

float UEnclaveGASHealthComponent::GetHealthValue() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return IsValid(ASC)
		? ASC->GetNumericAttribute(UEnclaveAttributeSet::GetHealthAttribute())
		: 0.0f;
}

float UEnclaveGASHealthComponent::GetMaxHealthValue() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return IsValid(ASC)
		? ASC->GetNumericAttribute(UEnclaveAttributeSet::GetMaxHealthAttribute())
		: 0.0f;
}

void UEnclaveGASHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	const FEnclaveDamageRequest* DamageRequest = ActiveDamageRequests.IsEmpty() ? nullptr : &ActiveDamageRequests.Last();
	NotifyHealthChanged(ChangeData.OldValue, ChangeData.NewValue, DamageRequest);
}

void UEnclaveGASHealthComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	NotifyMaxHealthChanged(ChangeData.OldValue, ChangeData.NewValue);
}
