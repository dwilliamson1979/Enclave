// Copyright Lightcarrier Studios


#include "Combat/EnclaveCombatComponent.h"

UEnclaveCombatComponent::UEnclaveCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEnclaveCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UEnclaveCombatComponent::Initiate_CycleWeapon()
{
}

void UEnclaveCombatComponent::Initiate_FireWeapon_Pressed()
{
}

void UEnclaveCombatComponent::Initiate_FireWeapon_Released()
{
}

void UEnclaveCombatComponent::Initiate_ReloadWeapon()
{
}