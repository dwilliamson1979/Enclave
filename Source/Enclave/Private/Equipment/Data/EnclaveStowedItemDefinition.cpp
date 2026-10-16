// Copyright Lightcarrier Studios
#include "Equipment/Data/EnclaveStowedItemDefinition.h"
#include "GameplayTags/EnclaveItemTags.h"

UEnclaveStowedItemDefinition::UEnclaveStowedItemDefinition()
{
	AddItemCapability(EnclaveGameplayTags::Item_Capability_Wieldable);
	SetHasPhysicalPresentation(false);
	SetItemType(EnclaveGameplayTags::Item_Type_Stowed);
}
