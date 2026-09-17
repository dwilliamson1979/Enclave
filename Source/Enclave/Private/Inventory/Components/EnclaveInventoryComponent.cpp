// Copyright Lightcarrier Studios

#include "Inventory/Components/EnclaveInventoryComponent.h"

#include "Equipment/Items/EnclaveItem.h"

#include "GameFramework/Pawn.h"

#include "Net/UnrealNetwork.h"

UEnclaveInventoryComponent::UEnclaveInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveInventoryComponent, Items);
}

AEnclaveItem* UEnclaveInventoryComponent::SpawnItem(TSubclassOf<AEnclaveItem> ItemClass)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !ItemClass)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnclaveItem* Item = World->SpawnActor<AEnclaveItem>(ItemClass, OwnerActor->GetActorTransform(), SpawnParameters);

	if (!IsValid(Item))
	{
		return nullptr;
	}

	// Inventory items remain hidden until the equipment system chooses to wield them.
	Item->SetActorHiddenInGame(true);

	if (!AddItem(Item))
	{
		Item->Destroy();
		return nullptr;
	}

	return Item;
}

bool UEnclaveInventoryComponent::AddItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Item))
	{
		return false;
	}

	if (Items.Contains(Item))
	{
		return false;
	}

	Items.Add(Item);

	return true;
}

bool UEnclaveInventoryComponent::RemoveItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Item))
	{
		return false;
	}

	return Items.Remove(Item) > 0;
}

bool UEnclaveInventoryComponent::ContainsItem(const AEnclaveItem* Item) const
{
	return IsValid(Item) && Items.Contains(Item);
}

AEnclaveItem* UEnclaveInventoryComponent::GetItemAt(int32 Index) const
{
	return Items.IsValidIndex(Index) ? Items[Index] : nullptr;
}
