// Fill out your copyright notice in the Description page of Project Settings.

#include "GameplayTags/EnclaveInputTags.h"

namespace EnclaveInputTags
{
	// Input - Avatar / Body Actions
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar, "Input.Avatar", "Root tag for avatar/body input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar_Move, "Input.Avatar.Move", "Movement input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar_Look, "Input.Avatar.Look", "Look input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar_Jump, "Input.Avatar.Jump", "Jump input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar_Crouch, "Input.Avatar.Crouch", "Crouch input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar_Sprint, "Input.Avatar.Sprint", "Sprint input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Avatar_Aim, "Input.Avatar.Aim", "Aim input.");

	// Input ItemActions
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Item, "Input.Item", "Root tag for currently equipped item input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Item_Primary, "Input.Item.Primary", "Primary action for the currently equipped item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Item_Secondary, "Input.Item.Secondary", "Secondary action for the currently equipped item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Item_Tertiary, "Input.Item.Tertiary", "Tertiary action for the currently equipped item.");

	// Equipment EquipmentActions
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Equipment, "Input.Equipment", "Root tag for equipment-selection input.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Equipment_Next, "Input.Equipment.Next", "Select the next available equipment item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Equipment_Previous, "Input.Equipment.Previous", "Select the previous available equipment item.");

	// Input - Gameplay / Item / Interaction Actions
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action, "Input.Action", "Root tag for gameplay action input.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item, "Input.Action.Item", "Root tag for equipped item and equipment-related input.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Primary, "Input.Action.Item.Primary", "Primary equipped item action, such as firing a weapon or using the main tool function.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Secondary, "Input.Action.Item.Secondary", "Secondary equipped item action, such as aiming, blocking, alternate fire, or secondary tool use.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Reload, "Input.Action.Item.Reload", "Reload or replenish the currently equipped item.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Aim, "Input.Action.Item.Aim", "Aim, focus, or precision-use input for the currently equipped item.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Next, "Input.Action.Item.Next", "Cycle to the next equipped item or item slot.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Previous, "Input.Action.Item.Previous", "Cycle to the previous equipped item or item slot.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Equip, "Input.Action.Item.Equip", "Equip an item or confirm item equip action.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Unequip, "Input.Action.Item.Unequip", "Unequip the currently equipped item.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Item_Drop, "Input.Action.Item.Drop", "Drop the currently equipped item.");
	//UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Action_Interact, "Input.Action.Interact", "Primary interaction input.");
}