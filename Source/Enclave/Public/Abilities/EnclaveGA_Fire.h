// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveGA_Fire.generated.h"

/**
 * Phase 1 stub. Exists so weapons have a concrete ability class to grant and
 * so grant/remove-on-equip can be tested end-to-end. Activation immediately
 * ends the ability - no trace, no damage, no cooldown yet.
 *
 * Phase 2 fills this in: hitscan trace, a damage Gameplay Effect, a
 * cooldown Gameplay Effect. Nothing about the grant/remove wiring in
 * EquipmentComponent should need to change when that happens.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Fire : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Fire();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
