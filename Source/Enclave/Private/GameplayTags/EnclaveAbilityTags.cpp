// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveAbilityTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action, "Ability.Type.Action");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Item, "Ability.Type.Action.Item");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Item_Fire, "Ability.Type.Action.Item.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Item_Reload, "Ability.Type.Action.Item.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Character, "Ability.Type.Action.Character");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Character_Aim, "Ability.Type.Action.Character.Aim");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Item_EquipDuration, "Data.Item.EquipDuration");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item, "Cooldown.Item", "Root for cooldown channels owned by item-supplied gameplay.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_Rifle, "Cooldown.Item.Rifle", "Cooldown channel namespace currently used by rifle items.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_Rifle_Fire, "Cooldown.Item.Rifle.Fire", "Fire cooldown channel currently used by rifle items.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_Pistol, "Cooldown.Item.Pistol", "Cooldown channel namespace currently used by pistol items.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Item_Pistol_Fire, "Cooldown.Item.Pistol.Fire", "Fire cooldown channel currently used by pistol items.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Item_Reloading, "State.Item.Reloading", "Observable state while the current item reload action is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Item_Equipping, "State.Item.Equipping", "Observable state while the current wield relationship is within its authoritative equip window.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Character_Aiming, "State.Character.Aiming", "Observable state while avatar-owned Character Aim is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Lifecycle_Dead, "State.Lifecycle.Dead", "Authoritative lifecycle state while the current avatar is dead.");
}