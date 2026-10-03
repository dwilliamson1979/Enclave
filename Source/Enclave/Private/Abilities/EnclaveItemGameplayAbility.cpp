// Copyright Lightcarrier Studios

#include "Abilities/EnclaveItemGameplayAbility.h"

#include "GameplayTags/EnclaveAbilityTags.h"

UEnclaveItemGameplayAbility::UEnclaveItemGameplayAbility()
{
	// Every ability granted by the wielded item participates in the same broad
	// GAS interaction category. Equip/death/etc. can therefore block or cancel
	// item actions generically without knowing about Fire, Reload, or future
	// concrete actions.
	AddAbilityAssetTag(EnclaveGameplayTags::Ability_Type_Action_Item);
}
