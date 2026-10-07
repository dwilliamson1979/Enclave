// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Health/Components/EnclaveHealthComponent.h"
#include "Combat/Interfaces/EnclaveDamageReceiverInterface.h"
#include "EnclaveGASHealthComponent.generated.h"

struct FEnclaveDamageRequest;
struct FOnAttributeChangeData;
class UAbilitySystemComponent;

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
class ENCLAVE_API UEnclaveGASHealthComponent : public UEnclaveHealthComponent, public IEnclaveDamageReceiverInterface
{
	GENERATED_BODY()

public:
	UEnclaveGASHealthComponent();

	/** Binds this component to the ASC that owns its health attributes. */
	void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent);

	/** Removes GAS attribute delegate bindings and clears the current ASC. */
	void UninitializeFromAbilitySystem();

	// IEnclaveDamageReceiverInterface
	virtual bool ReceiveDamage(const FEnclaveDamageRequest& DamageRequest) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual float GetHealthValue() const override;
	virtual float GetMaxHealthValue() const override;

private:
	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	FDelegateHandle HealthChangedDelegateHandle;
	FDelegateHandle MaxHealthChangedDelegateHandle;
};
