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
	/** Actions granted by the avatar independently of Equipment. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Character);
	/** Character-owned hold-to-aim; canceled by any actual wield change. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Type_Action_Character_Aim);

	/** Set-by-caller magnitude tag used by the generic Enclave damage GameplayEffect. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	/** Set-by-caller duration used by the current wield relationship's equipping Gameplay Effect. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Item_EquipDuration);

	/** Root for cooldown channels owned by item-supplied gameplay. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item);
	/** Rifle item cooldown channel. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item_Rifle);
	/** Fire cooldown channel currently used by rifle items. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item_Rifle_Fire);
	/** Pistol item cooldown channel. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item_Pistol);
	/** Fire cooldown channel currently used by pistol items. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Item_Pistol_Fire);

	/**
	 * Owned by the avatar's ASC for as long as UEnclaveGA_Reload is active.
	 * This is observable item-action state for animation/UI/debugging; Reload's
	 * ability-tag relationship owns the rule that Fire cannot begin during Reload.
	 */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_Reloading);

	/** Granted by the authoritative equip-window Gameplay Effect while the current wield relationship is not ready for item actions. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Item_Equipping);
	/** Observable GAS state while the character Aim ability is active. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Aiming);

	/** Granted while the current avatar is in the Dead lifecycle state. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Lifecycle_Dead);
}