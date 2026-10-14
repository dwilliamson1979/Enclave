// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveMeleeWeapon.h"

#include "Equipment/Weapons/Data/EnclaveMeleeWeaponDefinition.h"

const UEnclaveMeleeWeaponDefinition* AEnclaveMeleeWeapon::GetMeleeWeaponDefinition() const
{
	return Cast<UEnclaveMeleeWeaponDefinition>(GetWeaponDefinition());
}
