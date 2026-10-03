// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace EnclaveGameplayTags
{
	/** Broad classification used to coordinate interruptible/blockable gameplay actions through GAS. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action);
	/** All actions supplied by the currently wielded item. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Item);
	/** Wielded-item fire action. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Item_Fire);
	/** Wielded-item reload action. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Item_Reload);

	/** Set-by-caller magnitude tag used on damage Gameplay Effects (e.g. GE_Fire_Damage). */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	/** Set-by-caller duration used by the current wield relationship's equipping Gameplay Effect. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_EquipDuration);

	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Rifle_Fire);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Pistol_Fire);

	/**
	 * Owned by the avatar's ASC for as long as UEnclaveGA_Reload is active.
	 * This is observable item-action state for animation/UI/debugging; Reload's
	 * ability-tag relationship owns the rule that Fire cannot begin during Reload.
	 */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_Reloading);

	/** Granted by the authoritative equip-window Gameplay Effect while the current wield relationship is not ready for item actions. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_Equipping);
}