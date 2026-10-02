// Copyright Lightcarrier Studios

#include "Abilities/EnclaveItemGameplayAbility.h"

#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveItemGameplayAbility::UEnclaveItemGameplayAbility()
{
	// Equipment does not special-case Fire, Reload, or future item actions.
	// GAS rejects every item ability while the wield relationship is still
	// inside its authoritative equip window.
	ActivationBlockedTags.AddTag(EnclaveGameplayTags::State_Item_Wielded_Equipping);
}
