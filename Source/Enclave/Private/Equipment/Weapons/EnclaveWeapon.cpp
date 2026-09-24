// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveWeapon.h"

#include "Components/SkeletalMeshComponent.h"

#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

AEnclaveWeapon::AEnclaveWeapon()
{
}

void AEnclaveWeapon::BeginPlay()
{
	Super::BeginPlay();

	ApplyWeaponDefinition();
}

FGameplayTag AEnclaveWeapon::GetItemType() const
{
	if (!IsValid(WeaponDefinition))
	{
		return FGameplayTag();
	}

	return WeaponDefinition->GetWeaponType();
}

TArray<FEnclaveAbilityGrant> AEnclaveWeapon::GetAbilityGrants() const
{
	if (!IsValid(WeaponDefinition))
	{
		return {};
	}

	return WeaponDefinition->GetAbilityGrants();
}

void AEnclaveWeapon::ApplyWeaponDefinition()
{
	if (!IsValid(WeaponDefinition))
	{
		return;
	}

	FirstPersonMesh->SetSkeletalMesh(WeaponDefinition->GetFirstPersonMesh());
	ThirdPersonMesh->SetSkeletalMesh(WeaponDefinition->GetThirdPersonMesh());
}

bool AEnclaveWeapon::HandlePrimaryAction_Implementation(const FEnclaveInputEvent& InputEvent)
{
	return TryPrimaryAction(InputEvent);
}

bool AEnclaveWeapon::HandleSecondaryAction_Implementation(const FEnclaveInputEvent& InputEvent)
{
	return TrySecondaryAction(InputEvent);
}

bool AEnclaveWeapon::HandleTertiaryAction_Implementation(const FEnclaveInputEvent& InputEvent)
{
	return TryTertiaryAction(InputEvent);
}

bool AEnclaveWeapon::TryPrimaryAction(const FEnclaveInputEvent& InputEvent)
{
	return false;
}

bool AEnclaveWeapon::TrySecondaryAction(const FEnclaveInputEvent& InputEvent)
{
	return false;
}

bool AEnclaveWeapon::TryTertiaryAction(const FEnclaveInputEvent& InputEvent)
{
	return false;
}

//Adding this because I may reference it in the future. S2L14
//void AWeapon::OnRep_Instigator()
//{
//	Super::OnRep_Instigator();
//	AttachToOwningPawn();
//}
