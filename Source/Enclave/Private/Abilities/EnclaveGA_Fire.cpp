// Copyright Lightcarrier Studios

#include "Abilities/EnclaveGA_Fire.h"

UEnclaveGA_Fire::UEnclaveGA_Fire()
{
}

void UEnclaveGA_Fire::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	// Phase 1 stub: proves granting/activation wiring only. Real fire logic
	// (trace, damage effect, cooldown) arrives in Phase 2.
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/ true, /*bWasCancelled=*/ false);
}
