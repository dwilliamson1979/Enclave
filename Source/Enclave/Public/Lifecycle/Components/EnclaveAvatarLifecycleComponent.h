// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Abilities/Interfaces/EnclaveAbilitySystemConsumerInterface.h"
#include "EnclaveAvatarLifecycleComponent.generated.h"

class UAbilitySystemComponent;
class UEnclaveHealthComponent;

/**
 * Avatar-scoped bridge from health depletion into semantic lifecycle state.
 *
 * Health deliberately stops at "depleted" because zero health does not have
 * to mean Death for every actor or future game mode. This component owns the
 * current Enclave character policy: depleted Health transitions the avatar to
 * State.Lifecycle.Dead through GAS.
 *
 * The ASC is PlayerState-owned and survives Pawn replacement, so Dead also
 * survives unless the next avatar explicitly clears it. Each new instance of
 * this component resets the previous avatar's Dead effect exactly once when it
 * first binds to the persistent ASC. Rebinding the same Pawn does not revive it.
 */
UCLASS(Blueprintable, ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveAvatarLifecycleComponent : public UActorComponent, public IEnclaveAbilitySystemConsumerInterface
{
	GENERATED_BODY()

public:
	UEnclaveAvatarLifecycleComponent();

	// IEnclaveAbilitySystemConsumerInterface
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void UninitializeFromAbilitySystem() override;

	/** True while the bound ASC owns State.Lifecycle.Dead. */
	UFUNCTION(BlueprintPure, Category = "Lifecycle")
	bool IsDead() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleHealthDepleted(UEnclaveHealthComponent* DepletedHealthComponent);

	void InitializeAvatarLifecycleIfNeeded(UAbilitySystemComponent* InAbilitySystemComponent);
	void EnterDeadState();

	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	TWeakObjectPtr<UEnclaveHealthComponent> HealthComponent;

	/**
	 * Survives ASC unbinding so re-possessing this same Pawn does not clear Dead.
	 * A respawn creates a new component instance and therefore one fresh reset.
	 */
	bool bHasInitializedAvatarLifecycle = false;
};
