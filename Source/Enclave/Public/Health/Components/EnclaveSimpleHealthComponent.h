// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Health/Components/EnclaveHealthComponent.h"
#include "Combat/Interfaces/EnclaveDamageReceiverInterface.h"
#include "EnclaveSimpleHealthComponent.generated.h"

struct FEnclaveDamageRequest;

/**
 * Lightweight, non-GAS health implementation for simple damageable actors
 * such as crates, explosive props, and uncomplicated wildlife.
 *
 * Health and MaxHealth are owned directly by this component and replicated
 * by the owning Actor. This component also serves as the top-level Enclave
 * damage receiver for simple actors; callers depend on the damage receiver
 * interface rather than on this concrete component type.
 *
 * This component owns health accounting only. Reaching zero broadcasts
 * OnHealthDepleted; the owning actor decides whether that means break,
 * destroy, play a death animation, drop loot, etc.
 */
UCLASS(Blueprintable, ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveSimpleHealthComponent : public UEnclaveHealthComponent, public IEnclaveDamageReceiverInterface
{
	GENERATED_BODY()

public:
	UEnclaveSimpleHealthComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IEnclaveDamageReceiverInterface
	virtual bool ReceiveDamage(const FEnclaveDamageRequest& DamageRequest) override;

protected:
	virtual void BeginPlay() override;

	virtual float GetHealthValue() const override { return Health; }
	virtual float GetMaxHealthValue() const override { return MaxHealth; }

private:
	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void OnRep_MaxHealth(float OldMaxHealth);

	/** Direct health storage for this lightweight implementation. Server authoritative. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health", meta = (AllowPrivateAccess = "true"))
	float Health = 100.0f;

	/** Designer-authored maximum. This implementation always begins at MaxHealth. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float MaxHealth = 100.0f;
};
