// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveAbilityTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action, "Ability.Type.Action");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Item, "Ability.Type.Action.Item");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Item_Fire, "Ability.Type.Action.Item.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Type_Action_Item_Reload, "Ability.Type.Action.Item.Reload");
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage, "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Item_EquipDuration, "Data.Item.EquipDuration");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Weapon_Rifle_Fire, "Cooldown.Weapon.Rifle.Fire");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Weapon_Pistol_Fire, "Cooldown.Weapon.Pistol.Fire");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Item_Reloading, "State.Item.Reloading", "Observable state while the current item reload action is active.");
	UE_DEFINE_GAMEPLAY_TAG(State_Item_Wielded_Equipping, "State.Item.Wielded.Equipping");
}