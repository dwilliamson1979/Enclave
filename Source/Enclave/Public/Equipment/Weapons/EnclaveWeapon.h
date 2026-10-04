// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Items/EnclaveItem.h"
#include "EnclaveWeapon.generated.h"

class UEnclaveItemDefinition;
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
	AEnclaveWeapon();

	/**
	 * Temporary migration override: prefer AEnclaveItem::ItemDefinition, but
	 * fall back to the legacy WeaponDefinition property so existing weapon
	 * Blueprints continue working while their asset reference is migrated.
	 */
	virtual const UEnclaveItemDefinition* GetItemDefinition() const override;

	/** Generic item definition resolved as a weapon definition. */
	const UEnclaveWeaponDefinition* GetWeaponDefinition() const;

private:
	/**
	 * Legacy asset reference retained only for staged editor migration.
	 * Assign the inherited Item Definition property instead; remove this field
	 * after BP_Rifle/BP_Pistol (and any other weapon actors) are migrated.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Migration", meta = (AllowPrivateAccess = "true", DisplayName = "Weapon Definition (Legacy - Migrate)"))
	TObjectPtr<UEnclaveWeaponDefinition> WeaponDefinition;
};
