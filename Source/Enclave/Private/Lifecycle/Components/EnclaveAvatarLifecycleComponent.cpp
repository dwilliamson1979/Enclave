// Copyright Lightcarrier Studios

#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"

#include "Abilities/Effects/EnclaveGE_Dead.h"
#include "GameplayTags/EnclaveAbilityTags.h"
#include "Health/Components/EnclaveHealthComponent.h"

UEnclaveAvatarLifecycleComponent::UEnclaveAvatarLifecycleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveAvatarLifecycleComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UEnclaveHealthComponent* FoundHealthComponent = UEnclaveHealthComponent::FindHealthComponent(GetOwner()))
	{
		HealthComponent = FoundHealthComponent;
		FoundHealthComponent->OnHealthDepleted.AddUniqueDynamic(this, &UEnclaveAvatarLifecycleComponent::HandleHealthDepleted);
	}
}

void UEnclaveAvatarLifecycleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEnclaveHealthComponent* BoundHealthComponent = HealthComponent.Get())
	{
		BoundHealthComponent->OnHealthDepleted.RemoveDynamic(this, &UEnclaveAvatarLifecycleComponent::HandleHealthDepleted);
	}

	HealthComponent.Reset();
	UninitializeFromAbilitySystem();

	Super::EndPlay(EndPlayReason);
}

void UEnclaveAvatarLifecycleComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent)
{
	if (AbilitySystemComponent.Get() == InAbilitySystemComponent)
	{
		return;
	}

	UninitializeFromAbilitySystem();

	if (!IsValid(InAbilitySystemComponent))
	{
		return;
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	InitializeAvatarLifecycleIfNeeded(InAbilitySystemComponent);
}

void UEnclaveAvatarLifecycleComponent::UninitializeFromAbilitySystem()
{
	AbilitySystemComponent.Reset();
}

bool UEnclaveAvatarLifecycleComponent::IsDead() const
{
	const UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();
	return IsValid(ASC) && ASC->HasMatchingGameplayTag(EnclaveGameplayTags::State_Lifecycle_Dead);
}

void UEnclaveAvatarLifecycleComponent::HandleHealthDepleted(UEnclaveHealthComponent* DepletedHealthComponent)
{
	if (!IsValid(DepletedHealthComponent) || DepletedHealthComponent != HealthComponent.Get())
	{
		return;
	}

	EnterDeadState();
}

void UEnclaveAvatarLifecycleComponent::InitializeAvatarLifecycleIfNeeded(UAbilitySystemComponent* InAbilitySystemComponent)
{
	AActor* Owner = GetOwner();

	if (bHasInitializedAvatarLifecycle || !IsValid(Owner) || !Owner->HasAuthority() || !IsValid(InAbilitySystemComponent))
	{
		return;
	}

	bHasInitializedAvatarLifecycle = true;

	// Dead belongs to the previous avatar but the ASC belongs to PlayerState and
	// persists across respawn. A genuinely new avatar explicitly clears only the
	// lifecycle GE it is replacing; other persistent ASC state is left alone.
	InAbilitySystemComponent->RemoveActiveGameplayEffectBySourceEffect(UEnclaveGE_Dead::StaticClass(), nullptr, -1);
}

void UEnclaveAvatarLifecycleComponent::EnterDeadState()
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();

	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(ASC) || IsDead())
	{
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ASC->ApplyGameplayEffectToSelf(UEnclaveGE_Dead::StaticClass()->GetDefaultObject<UGameplayEffect>(), 1.0f, ContextHandle);
}
