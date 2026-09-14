// Copyright Lightcarrier Studios

#include "Equipment/Weapons/EnclaveWeapon.h"

#include "Components/SkeletalMeshComponent.h"

#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

AEnclaveWeapon::AEnclaveWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	SetRootComponent(FirstPersonMesh);
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	FirstPersonMesh->SetReceivesDecals(false);
	FirstPersonMesh->SetCastShadow(false);
	FirstPersonMesh->SetHiddenInGame(true);

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ThirdPersonMesh"));
	ThirdPersonMesh->SetupAttachment(RootComponent);
	ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	ThirdPersonMesh->SetReceivesDecals(false);
	ThirdPersonMesh->SetCastShadow(true);
	ThirdPersonMesh->SetHiddenInGame(true);
}

void AEnclaveWeapon::BeginPlay()
{
	Super::BeginPlay();

	ApplyWeaponDefinition();
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