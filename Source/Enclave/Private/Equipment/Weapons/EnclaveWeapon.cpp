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

float AEnclaveWeapon::GetEquipDuration() const
{
	return IsValid(WeaponDefinition) ? WeaponDefinition->GetEquipDuration() : Super::GetEquipDuration();
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