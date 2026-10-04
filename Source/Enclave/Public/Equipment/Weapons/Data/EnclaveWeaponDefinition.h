// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Data/EnclaveItemDefinition.h"
#include "GameplayTagContainer.h"
#include "EnclaveWeaponDefinition.generated.h"

/**
 * Weapon-specific authored data shared by every weapon definition.
 *
 * Generic item identity/capabilities, presentation meshes, equip timing, and
 * granted abilities live on UEnclaveItemDefinition. This class adds only data
 * whose semantics are specifically weapon-level. Firearm-only combat and
 * presentation tuning lives one level further down on
 * UEnclaveFirearmWeaponDefinition.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveWeaponDefinition : public UEnclaveItemDefinition
{
	GENERATED_BODY()

public:
	UEnclaveWeaponDefinition();

	const FGameplayTagContainer& GetWeaponTags() const { return WeaponTags; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Classification", meta = (AllowPrivateAccess = "true", Categories = "Weapon"))
	FGameplayTagContainer WeaponTags;
};
