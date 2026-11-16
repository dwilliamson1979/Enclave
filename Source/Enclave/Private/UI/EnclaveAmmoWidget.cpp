// Copyright Lightcarrier Studios

#include "UI/EnclaveAmmoWidget.h"

#include "Components/TextBlock.h"
#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "Equipment/Weapons/EnclaveFirearmWeapon.h"
#include "GameFramework/Pawn.h"

void UEnclaveAmmoWidget::NativeDestruct()
{
	BindEquipmentComponent(nullptr);

	Super::NativeDestruct();
}

void UEnclaveAmmoWidget::OnPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindEquipmentComponent(NewPawn ? NewPawn->FindComponentByClass<UEnclaveEquipmentComponent>() : nullptr);

	Super::OnPawnChanged(OldPawn, NewPawn);
}

void UEnclaveAmmoWidget::BindEquipmentComponent(UEnclaveEquipmentComponent* NewEquipmentComponent)
{
	if (UEnclaveEquipmentComponent* OldEquipmentComponent = EquipmentComponent.Get())
	{
		OldEquipmentComponent->OnWieldedItemChanged.RemoveDynamic(this, &UEnclaveAmmoWidget::HandleWieldedItemChanged);
	}

	EquipmentComponent = NewEquipmentComponent;
	if (NewEquipmentComponent)
	{
		NewEquipmentComponent->OnWieldedItemChanged.AddUniqueDynamic(this, &UEnclaveAmmoWidget::HandleWieldedItemChanged);
	}

	// Follow what is already wielded; the event only covers what follows.
	BindWeapon(NewEquipmentComponent ? Cast<AEnclaveFirearmWeapon>(NewEquipmentComponent->GetCurrentWieldedItem()) : nullptr);
}

void UEnclaveAmmoWidget::BindWeapon(AEnclaveFirearmWeapon* NewWeapon)
{
	if (AEnclaveFirearmWeapon* OldWeapon = Weapon.Get())
	{
		OldWeapon->OnMagazineAmmoChanged.RemoveDynamic(this, &UEnclaveAmmoWidget::HandleMagazineAmmoChanged);
	}

	Weapon = NewWeapon;
	if (NewWeapon)
	{
		NewWeapon->OnMagazineAmmoChanged.AddUniqueDynamic(this, &UEnclaveAmmoWidget::HandleMagazineAmmoChanged);
	}

	Refresh();
}

void UEnclaveAmmoWidget::Refresh()
{
	const AEnclaveFirearmWeapon* CurrentWeapon = Weapon.Get();
	const int32 Capacity = CurrentWeapon ? CurrentWeapon->GetMagazineCapacity() : 0;
	if (Capacity <= 0)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (AmmoText)
	{
		AmmoText->SetText(FText::AsNumber(CurrentWeapon->GetCurrentMagazineAmmo()));
	}

	if (CapacityText)
	{
		CapacityText->SetText(FText::AsNumber(Capacity));
	}
}

void UEnclaveAmmoWidget::HandleWieldedItemChanged(UEnclaveEquipmentComponent* InEquipmentComponent, AEnclaveItem* OldItem, AEnclaveItem* NewItem)
{
	BindWeapon(Cast<AEnclaveFirearmWeapon>(NewItem));
}

void UEnclaveAmmoWidget::HandleMagazineAmmoChanged(AEnclaveFirearmWeapon* InWeapon, int32 OldRounds, int32 NewRounds)
{
	Refresh();
}
