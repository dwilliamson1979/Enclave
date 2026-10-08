// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/EnclaveDamageRequest.h"
#include "EnclaveHealthComponent.generated.h"

class AActor;
class UEnclaveHealthComponent;

/** Fired whenever the authoritative health value changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnclaveHealthChangedSignature, UEnclaveHealthComponent*, HealthComponent, float, OldHealth, float, NewHealth);

/** Fired once for each transition from positive health to zero health. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEnclaveHealthDepletedSignature, UEnclaveHealthComponent*, HealthComponent, FEnclaveDamageRequest, DamageRequest);

/** Fired whenever the maximum-health value changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEnclaveMaxHealthChangedSignature, UEnclaveHealthComponent*, HealthComponent, float, OldMaxHealth, float, NewMaxHealth);

/**
 * Storage-agnostic health semantics shared by simple world actors and
 * GAS-backed combatants.
 *
 * This component deliberately does not own Health or MaxHealth. Derived
 * implementations decide where those authoritative values live:
 * UEnclaveSimpleHealthComponent stores/replicates them directly, while the
 * future UEnclaveGASHealthComponent will expose values owned by an Attribute
 * Set. Consumers can depend on this base without caring which storage model
 * backs the actor.
 *
 * The base also stops at "health depleted." It does not equate zero health
 * with Death. A crate may break, an animal may die, and a future player mode
 * may enter an incapacitated state. That lifecycle policy belongs above the
 * shared health abstraction.
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Enclave))
class ENCLAVE_API UEnclaveHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnclaveHealthComponent();

	/** Finds the actor's health abstraction without exposing its concrete storage model. */
	UFUNCTION(BlueprintPure, Category = "Health", meta = (DefaultToSelf = "Actor"))
	static UEnclaveHealthComponent* FindHealthComponent(AActor* Actor);

	/** Current authoritative health value exposed by the concrete implementation. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const;

	/** Current authoritative maximum-health value exposed by the concrete implementation. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const;

	/** Health normalized to [0,1]. Returns 0 when MaxHealth is not positive. */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealthNormalized() const;

	/** True while this health model still has positive health remaining. */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool HasHealthRemaining() const;

	/** True when health has reached zero. This is not synonymous with "dead." */
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsHealthDepleted() const;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FEnclaveHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FEnclaveHealthDepletedSignature OnHealthDepleted;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FEnclaveMaxHealthChangedSignature OnMaxHealthChanged;

protected:
	/** Storage hook implemented by each concrete health model. */
	virtual float GetHealthValue() const PURE_VIRTUAL(UEnclaveHealthComponent::GetHealthValue, return 0.0f;);

	/** Storage hook implemented by each concrete health model. */
	virtual float GetMaxHealthValue() const PURE_VIRTUAL(UEnclaveHealthComponent::GetMaxHealthValue, return 0.0f;);

	/** Shared notification path used by concrete implementations on authority and from RepNotifies. */
	void NotifyHealthChanged(float OldHealth, float NewHealth, const FEnclaveDamageRequest* DamageRequest = nullptr);

	/** Shared notification path used by concrete implementations on authority and from RepNotifies. */
	void NotifyMaxHealthChanged(float OldMaxHealth, float NewMaxHealth);
};
