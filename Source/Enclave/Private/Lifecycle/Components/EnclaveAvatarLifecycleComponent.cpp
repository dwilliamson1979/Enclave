// Copyright Lightcarrier Studios

#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "Abilities/Effects/EnclaveGE_Dead.h"
#include "Abilities/Effects/EnclaveGameplayEffectScopePolicy.h"
#include "GameplayTags/EnclaveAbilityTags.h"
#include "Health/Components/EnclaveHealthComponent.h"

UEnclaveAvatarLifecycleComponent::UEnclaveAvatarLifecycleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEnclaveAvatarLifecycleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveAvatarLifecycleComponent, LifecycleState);
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
	return LifecycleState == EEnclaveAvatarLifecycleState::Dead;
}

void UEnclaveAvatarLifecycleComponent::HandleHealthDepleted(UEnclaveHealthComponent* DepletedHealthComponent, FEnclaveDamageRequest DamageRequest)
{
	if (!IsValid(DepletedHealthComponent) || DepletedHealthComponent != HealthComponent.Get())
	{
		return;
	}

	EnterDeadState(DamageRequest);
}

void UEnclaveAvatarLifecycleComponent::InitializeAvatarLifecycleIfNeeded(UAbilitySystemComponent* InAbilitySystemComponent)
{
	AActor* Owner = GetOwner();

	if (bHasInitializedAvatarLifecycle || !IsValid(Owner) || !Owner->HasAuthority() || !IsValid(InAbilitySystemComponent))
	{
		return;
	}

	bHasInitializedAvatarLifecycle = true;

	// Persistent-ASC effect cleanup is owned by the Character's ASC/avatar
	// initialization boundary and runs before consumers are initialized. This
	// component owns only this body's replicated lifecycle identity.
	SetLifecycleState(EEnclaveAvatarLifecycleState::Alive);
}

void UEnclaveAvatarLifecycleComponent::EnterDeadState(const FEnclaveDamageRequest& DamageRequest)
{
	AActor* Owner = GetOwner();
	UAbilitySystemComponent* ASC = AbilitySystemComponent.Get();

	if (!IsValid(Owner) || !Owner->HasAuthority() || !IsValid(ASC) || IsDead())
	{
		return;
	}

	// Body identity is avatar-owned and survives ASC unbinding. Listeners such
	// as the SSR target history react to OnLifecycleStateChanged here.
	SetLifecycleState(EEnclaveAvatarLifecycleState::Dead);

	// End this body's transient GAS state immediately. Run the sweep before
	// GE_Dead is applied so GE_Dead itself requires no special-case exclusion.
	// Player-scoped effects are the only active effects allowed to survive.
	FEnclaveGameplayEffectScopePolicy::RemoveAvatarLifetimeEffects(
		ASC,
		EEnclaveAvatarEffectCleanupReason::Death);

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();

	if (IsValid(DamageRequest.SourceActor))
	{
		ContextHandle.AddInstigator(DamageRequest.SourceActor.Get(), DamageRequest.DamageCauser.Get());
	}

	if (IsValid(DamageRequest.DamageCauser))
	{
		ContextHandle.AddSourceObject(DamageRequest.DamageCauser.Get());
	}

	if (DamageRequest.HitResult.bBlockingHit)
	{
		ContextHandle.AddHitResult(DamageRequest.HitResult);
	}

	ASC->ApplyGameplayEffectToSelf(UEnclaveGE_Dead::StaticClass()->GetDefaultObject<UGameplayEffect>(), 1.0f, ContextHandle);
	OnAvatarDied.Broadcast(this, DamageRequest);
}

void UEnclaveAvatarLifecycleComponent::SetLifecycleState(EEnclaveAvatarLifecycleState NewState)
{
	if (LifecycleState == NewState)
	{
		return;
	}

	const EEnclaveAvatarLifecycleState OldState = LifecycleState;
	LifecycleState = NewState;
	HandleLifecycleStateChanged(OldState, NewState);
}

void UEnclaveAvatarLifecycleComponent::OnRep_LifecycleState(EEnclaveAvatarLifecycleState OldState)
{
	HandleLifecycleStateChanged(OldState, LifecycleState);
}

void UEnclaveAvatarLifecycleComponent::HandleLifecycleStateChanged(EEnclaveAvatarLifecycleState OldState, EEnclaveAvatarLifecycleState NewState)
{
	if (NewState == EEnclaveAvatarLifecycleState::Dead)
	{
		ApplyDeadMovementPolicy();
	}

	LifecycleStateChanged.Broadcast(OldState, NewState);
}

void UEnclaveAvatarLifecycleComponent::ApplyDeadMovementPolicy()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character))
	{
		return;
	}

	Character->StopJumping();

	if (UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}
}
