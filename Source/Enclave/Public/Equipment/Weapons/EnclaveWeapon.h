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
 * actions into weapon-specific behavior.
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

private:

	void ApplyWeaponDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveWeaponDefinition> WeaponDefinition;

};