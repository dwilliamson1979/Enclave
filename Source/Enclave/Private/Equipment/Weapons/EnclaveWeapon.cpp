// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveWeapon.h"

#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

const UEnclaveWeaponDefinition* AEnclaveWeapon::GetWeaponDefinition() const
{
	return Cast<UEnclaveWeaponDefinition>(GetItemDefinition());
}
