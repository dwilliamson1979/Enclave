// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Health/Components/EnclaveHealthComponent.h"
#include "Combat/Interfaces/EnclaveDamageReceiverInterface.h"
#include "Abilities/Interfaces/EnclaveAbilitySystemConsumerInterface.h"
#include "EnclaveGASHealthComponent.generated.h"

struct FEnclaveDamageRequest;
struct FOnAttributeChangeData;
class UAbilitySystemComponent;
class UEnclaveAttributeSet;

/**
 * GAS-backed implementation of the shared Enclave health abstraction.
 *
 * This component does not own Health or MaxHealth. Those authoritative values
 * live in UEnclaveAttributeSet on an Ability System Component. The component
 * exposes the same health semantics as UEnclaveSimpleHealthComponent, listens
 * to GAS attribute-change delegates, and currently serves as the top-level
 * damage receiver for a straightforward GAS combatant.
 *
 * If a future combatant needs shields/armor/routing ahead of Health, a
 * dedicated damage-router component can replace this as the top-level
 * IEnclaveDamageReceiverInterface without changing the health abstraction.
 */
UCLASS(Blueprintable, ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveGASHealthComponent : public UEnclaveHealthComponent, public IEnclaveDamageReceiverInterface, public IEnclaveAbilitySystemConsumerInterface
{
	GENERATED_BODY()

public:
	UEnclaveGASHealthComponent();

	// IEnclaveAbilitySystemConsumerInterface
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent) override;
	virtual void UninitializeFromAbilitySystem() override;

	// IEnclaveDamageReceiverInterface
	virtual bool ReceiveDamage(const FEnclaveDamageRequest& DamageRequest) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual float GetHealthValue() const override;
	virtual float GetMaxHealthValue() const override;

private:
	void InitializeAvatarHealthIfNeeded(UAbilitySystemComponent* InAbilitySystemComponent, const UEnclaveAttributeSet* AttributeSet);
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	/** Non-owning cache of the ASC this avatar component is currently bound to. */
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/**
	 * True once this component instance has initialized the Health for its avatar.
	 * Intentionally survives ASC unbinding so re-possessing the same Pawn does not heal it.
	 * A respawn creates a new Pawn/component instance and therefore gets one fresh initialization.
	 */
	bool bHasInitializedAvatarHealth = false;

	/** Operation-scoped requests currently being applied; the top entry supplies depletion attribution. */
	TArray<FEnclaveDamageRequest> ActiveDamageRequests;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
};
