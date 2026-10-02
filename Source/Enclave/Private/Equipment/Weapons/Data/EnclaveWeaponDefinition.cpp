// Copyright Lightcarrier Studios

#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

#include "GameplayTags/EnclaveItemTags.h"

UEnclaveWeaponDefinition::UEnclaveWeaponDefinition()
{
	AddItemCapability(EnclaveGameplayTags::Item_Capability_Wieldable);
}
