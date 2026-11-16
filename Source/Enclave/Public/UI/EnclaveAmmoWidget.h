// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveAmmoWidget.generated.h"

class AEnclaveFirearmWeapon;
class AEnclaveItem;
class UEnclaveEquipmentComponent;
class UTextBlock;

/**
 * Rounds in the wielded firearm's magazine (the predicted count on the owning
 * client, so it drops on the click). Follows the pawn's equipment component
 * to the wielded item, then that item's OnMagazineAmmoChanged. Hidden with no
 * pawn, no wielded firearm, or a firearm with no magazine.
 *
 * Magazine only: reserve ammo is unlimited until the per-ammo-type pool exists.
 * Reads the firearm directly, as the only item with a count; a read-side
 * interface comes when a second kind of counted item does.
 *
 * The Widget Blueprint must contain a TextBlock named AmmoText, and may
 * contain a TextBlock named CapacityText.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveAmmoWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void OnPawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CapacityText;

private:
	void BindEquipmentComponent(UEnclaveEquipmentComponent* NewEquipmentComponent);
	void BindWeapon(AEnclaveFirearmWeapon* NewWeapon);
	void Refresh();

	UFUNCTION()
	void HandleWieldedItemChanged(UEnclaveEquipmentComponent* InEquipmentComponent, AEnclaveItem* OldItem, AEnclaveItem* NewItem);

	UFUNCTION()
	void HandleMagazineAmmoChanged(AEnclaveFirearmWeapon* InWeapon, int32 OldRounds, int32 NewRounds);

	TWeakObjectPtr<UEnclaveEquipmentComponent> EquipmentComponent;
	TWeakObjectPtr<AEnclaveFirearmWeapon> Weapon;
};
