// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveItemGameplayAbility.generated.h"

/**
 * Base class for Gameplay Abilities granted by the currently wielded item.
 *
 * Every item ability is classified as Ability.Type.Action.Item. Gameplay
 * Effects and other abilities can use that identity to block/cancel the whole
 * item-action family without depending on concrete ability classes. Individual
 * abilities remain responsible for their additional action-specific rules.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveItemGameplayAbility : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveItemGameplayAbility();

protected:
	/**
	 * Item-supplied cooldowns belong to the current avatar/item relationship.
	 * Tag the outgoing cooldown spec as avatar-scoped even when the authored
	 * cooldown GE is a generic Blueprint asset.
	 */
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
