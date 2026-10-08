// Copyright Lightcarrier Studios

#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "Abilities/Effects/EnclaveGE_Dead.h"
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

	// Dead belongs to the previous avatar but the ASC belongs to PlayerState and
	// persists across respawn. A genuinely new avatar explicitly clears the
	// previous avatar's Dead effect. The new component's body-owned state begins
	// Alive independently of the shared ASC.
	InAbilitySystemComponent->RemoveActiveGameplayEffectBySourceEffect(UEnclaveGE_Dead::StaticClass(), nullptr, -1);
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

	// Body identity is avatar-owned and survives ASC unbinding. GAS still owns
	// the semantic action-cancel/block state for the currently controlled avatar.
	SetLifecycleState(EEnclaveAvatarLifecycleState::Dead);

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
