// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "EnclaveWeapon.generated.h"

class USkeletalMeshComponent;

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
class ENCLAVE_API AEnclaveWeapon : public AActor, public IEnclaveWieldableInterface
{
	GENERATED_BODY()

public:
	AEnclaveWeapon();

	// IEnclaveWieldableInterface
	virtual bool HandlePrimaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;
	virtual bool HandleSecondaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;
	virtual bool HandleTertiaryAction_Implementation(const FEnclaveInputEvent& InputEvent) override;

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }

protected:
	/**
	 * Attempts to perform the weapon's primary action.
	 *
	 * For a firearm this will normally mean firing, but derived weapon types
	 * remain free to interpret the action differently.
	 */
	virtual bool TryPrimaryAction(const FEnclaveInputEvent& InputEvent);

	/**
	 * Attempts to perform the weapon's secondary action.
	 */
	virtual bool TrySecondaryAction(const FEnclaveInputEvent& InputEvent);

	/**
	 * Attempts to perform the weapon's tertiary action.
	 */
	virtual bool TryTertiaryAction(const FEnclaveInputEvent& InputEvent);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

private:

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Tags", meta = (Categories = "Weapon"))
	FGameplayTag WeaponTypeTag;
};