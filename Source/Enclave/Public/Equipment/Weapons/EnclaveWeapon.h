// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Items/EnclaveItem.h"
#include "EnclaveWeapon.generated.h"

class UEnclaveWeaponDefinition;

/**
 * Base class for weapons.
 *
 * Generic physical-item configuration and behavior live on AEnclaveItem and
 * UEnclaveItemDefinition. This type exists as the runtime weapon tier for
 * behavior that is meaningful to weapons generally, while firearm-specific
 * hit detection, ammo/reload state, and presentation remain on
 * AEnclaveFirearmWeapon.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveWeapon : public AEnclaveItem
{
	GENERATED_BODY()

public:
	/** ItemDefinition, resolved as this actor's weapon-definition subtype. */
	const UEnclaveWeaponDefinition* GetWeaponDefinition() const;
};
