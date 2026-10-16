// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveEquipmentSlotTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Default, "Equipment.Slot.Default", "Fallback selection slot; item lifetime is owned externally.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Primary, "Equipment.Slot.Primary", "First ordinary wieldable slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Secondary, "Equipment.Slot.Secondary", "Second ordinary wieldable slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Tertiary, "Equipment.Slot.Tertiary", "Third ordinary wieldable slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Equipment_Slot_Stowed, "Equipment.Slot.Stowed", "Optional persistent stowed item, not included in the wield cycle.");
}
