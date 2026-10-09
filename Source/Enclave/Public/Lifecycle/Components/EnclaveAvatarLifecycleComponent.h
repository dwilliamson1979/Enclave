// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Abilities/Interfaces/EnclaveAbilitySystemConsumerInterface.h"
#include "Combat/EnclaveDamageRequest.h"
#include "EnclaveAvatarLifecycleComponent.generated.h"

class UAbilitySystemComponent;
class UEnclaveHealthComponent;
class UEnclaveAvatarLifecycleComponent;


UENUM(BlueprintType)
enum class EEnclaveAvatarLifecycleState : uint8
{
	Alive,
	Dead
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnclaveAvatarDiedSignature, UEnclaveAvatarLifecycleComponent*, LifecycleComponent, FEnclaveDamageRequest, DamageRequest);

/**
 * Avatar-scoped bridge from health depletion into semantic lifecycle state.
 *
 * Health deliberately stops at "depleted" because zero health does not have
 * to mean Death for every actor or future game mode. This component owns the
 * current Enclave character policy: depleted Health transitions the avatar to
 * State.Lifecycle.Dead through GAS.
 *
 * The ASC is PlayerState-owned and survives Pawn replacement. Avatar-lifetime
 * Gameplay Effects are swept immediately on death and again defensively before
 * a replacement avatar binds. Only explicitly Player-scoped active effects may
 * survive that boundary. This component independently owns this body's replicated
 * lifecycle identity.
 */
UCLASS(Blueprintable, ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveAvatarLifecycleComponent : public UActorComponent, public IEnclaveAbilitySystemConsumerInterface
{
	GENERATED_BODY()

public:
	UEnclaveAvatarLifecycleComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IEnclaveAbilitySystemConsumerInterface
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void UninitializeFromAbilitySystem() override;

	/** Current body-owned lifecycle state. This remains valid even after ASC unbinding. */
	UFUNCTION(BlueprintPure, Category = "Lifecycle")
	EEnclaveAvatarLifecycleState GetLifecycleState() const { return LifecycleState; }

	/** True while this specific avatar/body is dead. */
	UFUNCTION(BlueprintPure, Category = "Lifecycle")
	bool IsDead() const;

	/** Authority-side semantic death notification carrying the terminal damage request. */
	UPROPERTY(BlueprintAssignable, Category = "Lifecycle")
	FEnclaveAvatarDiedSignature OnAvatarDied;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleHealthDepleted(UEnclaveHealthComponent* DepletedHealthComponent, FEnclaveDamageRequest DamageRequest);

	void InitializeAvatarLifecycleIfNeeded(UAbilitySystemComponent* InAbilitySystemComponent);
	void EnterDeadState(const FEnclaveDamageRequest& DamageRequest);
	void SetLifecycleState(EEnclaveAvatarLifecycleState NewState);
	void HandleLifecycleStateChanged(EEnclaveAvatarLifecycleState OldState, EEnclaveAvatarLifecycleState NewState);
	void ApplyDeadMovementPolicy();

	UFUNCTION()
	void OnRep_LifecycleState(EEnclaveAvatarLifecycleState OldState);

	UPROPERTY(ReplicatedUsing = OnRep_LifecycleState)
	EEnclaveAvatarLifecycleState LifecycleState = EEnclaveAvatarLifecycleState::Alive;

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UEnclaveHealthComponent> HealthComponent;

	/**
	 * Survives ASC unbinding so re-possessing this same Pawn does not reset the
	 * body-owned lifecycle state. A respawn creates a new component instance.
	 */
	bool bHasInitializedAvatarLifecycle = false;
};
