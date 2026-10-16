// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveItemTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(
		Item_Capability_Wieldable,
		"Item.Capability.Wieldable",
		"Item may participate in the wield/equipment system."
	);
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Item_Type_Stowed, "Item.Type.Stowed", "Optional meshless Stowed item for avatar presentation.");
}
