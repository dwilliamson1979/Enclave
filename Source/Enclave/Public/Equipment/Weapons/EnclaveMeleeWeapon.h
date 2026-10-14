// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Weapons/EnclaveWeapon.h"
#include "EnclaveMeleeWeapon.generated.h"

class UEnclaveMeleeWeaponDefinition;

/** Runtime tier for melee weapons. No melee combat behavior is assumed yet. */
UCLASS(Blueprintable)
class ENCLAVE_API AEnclaveMeleeWeapon : public AEnclaveWeapon
{
	GENERATED_BODY()

public:
	const UEnclaveMeleeWeaponDefinition* GetMeleeWeaponDefinition() const;
};
