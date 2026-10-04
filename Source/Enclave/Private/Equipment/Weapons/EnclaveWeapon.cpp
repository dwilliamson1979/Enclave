// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveWeapon.h"

#include "Equipment/Data/EnclaveItemDefinition.h"
#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

AEnclaveWeapon::AEnclaveWeapon()
{
}

const UEnclaveItemDefinition* AEnclaveWeapon::GetItemDefinition() const
{
	if (const UEnclaveItemDefinition* Definition = Super::GetItemDefinition())
	{
		return Definition;
	}

	return WeaponDefinition;
}

const UEnclaveWeaponDefinition* AEnclaveWeapon::GetWeaponDefinition() const
{
	return Cast<UEnclaveWeaponDefinition>(GetItemDefinition());
}
