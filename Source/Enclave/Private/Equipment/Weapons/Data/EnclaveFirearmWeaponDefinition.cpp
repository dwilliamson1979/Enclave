// Copyright Lightcarrier Studios

#include "Equipment/Weapons/Data/EnclaveFirearmWeaponDefinition.h"

#include "GameplayTags/EnclaveDamageTags.h"

UEnclaveFirearmWeaponDefinition::UEnclaveFirearmWeaponDefinition()
{
	DamageTags.AddTag(EnclaveGameplayTags::Damage_Type_Ballistic);
}
