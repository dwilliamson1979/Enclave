// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Items/EnclaveItem.h"
#include "EnclaveWeapon.generated.h"

class UEnclaveWeaponDefinition;

/**
 * Base class for wieldable weapons.
 *
 * Owns weapon-level identity, configuration, equip timing, capabilities,
 * and ability grants. Item action support and execution are resolved through
 * GAS from the currently wielded item's granted abilities.
 *
 * Anything that only makes sense for a ranged/firearm weapon (hit detection,
 * ammo/reload state, muzzle presentation, and firearm-local action
 * presentation) lives one level down on AEnclaveFirearmWeapon, so a future
 * non-firearm weapon such as a knife or baton does not inherit firearm-only
 * behavior.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveWeapon : public AEnclaveItem
{
	GENERATED_BODY()

public:

	AEnclaveWeapon();

	virtual FGameplayTag GetItemType() const override;
	virtual bool HasItemCapability(const FGameplayTag& CapabilityTag) const override;
	virtual float GetEquipDuration() const override;
	virtual TArray<FEnclaveAbilityGrant> GetAbilityGrants() const override;

	const UEnclaveWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

protected:

	virtual void BeginPlay() override;

	/**
	 * Applies WeaponDefinition's shared presentation (currently: first- and
	 * third-person meshes) to this actor. Virtual so a subclass
	 * (AEnclaveFirearmWeapon) can extend it for its own definition subtype's
	 * data (muzzle sockets, etc.) - an override must call Super:: first.
	 */
	virtual void ApplyWeaponDefinition();

private:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveWeaponDefinition> WeaponDefinition;

};
