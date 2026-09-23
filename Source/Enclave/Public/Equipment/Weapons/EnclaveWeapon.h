// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "EnclaveWeapon.generated.h"

class UEnclaveWeaponDefinition;

/**
 * Base class for wieldable weapons.
 *
 * Defines common weapon representation and translates generic wieldable
 * actions into weapon-specific behavior. Deliberately minimal - anything
 * that only makes sense for a *ranged* weapon (fire hit-detection, muzzle
 * presentation, the Weapon.Fire/Weapon.Reload gameplay cue response) lives
 * one level down, on AEnclaveFirearmWeapon (split out 2026-09-23), so a
 * future non-firearm wieldable (a knife, a baton) doesn't inherit a
 * Fireable contract or muzzle sockets it has no use for.
 *
 * Networking and detailed firing/reloading mechanics are intentionally left
 * to more specialized weapon implementations as those systems are introduced.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveWeapon : public AEnclaveItem, public IEnclaveWieldableInterface
{
	GENERATED_BODY()

public:

	AEnclaveWeapon();

	virtual FGameplayTag GetItemType() const override;
	virtual TArray<TSubclassOf<UGameplayAbility>> GetGrantedAbilityClasses() const override;

	// IEnclaveWieldableInterface
	virtual bool HandlePrimaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;
	virtual bool HandleSecondaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;
	virtual bool HandleTertiaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;

	const UEnclaveWeaponDefinition* GetWeaponDefinition() const { return WeaponDefinition; }

protected:

	virtual void BeginPlay() override;

	virtual bool TryPrimaryAction(const FEnclaveInputEvent& InputEvent);
	virtual bool TrySecondaryAction(const FEnclaveInputEvent& InputEvent);
	virtual bool TryTertiaryAction(const FEnclaveInputEvent& InputEvent);

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
