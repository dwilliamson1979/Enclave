// Copyright Lightcarrier Studios

#include "Inventory/Components/EnclaveInventoryComponent.h"

#include "Equipment/Items/EnclaveItem.h"

#include "GameFramework/Pawn.h"

#include "Net/UnrealNetwork.h"

UEnclaveInventoryComponent::UEnclaveInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveInventoryComponent::UninitializeComponent()
{
	AActor* OwnerActor = GetOwner();

	// Carried items are Pawn-scoped in the current slice. Destroy them when the
	// owning Pawn is torn down so they cannot survive into a later respawn.
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		DestroyAllItems();
	}

	Super::UninitializeComponent();
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

void UEnclaveInventoryComponent::DestroyAllItems()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (Items.IsEmpty())
	{
		return;
	}

	// Publish the source change before destroying the actors. Equipment can then
	// cancel/revoke any current item ability while its SourceObject is still valid.
	TArray<TObjectPtr<AEnclaveItem>> ItemsToDestroy = MoveTemp(Items);
	Items.Reset();
	ItemSourceChangedDelegate.Broadcast();

	for (AEnclaveItem* Item : ItemsToDestroy)
	{
		if (IsValid(Item))
		{
			Item->Destroy();
		}
	}
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

	ItemSourceChangedDelegate.Broadcast();

	return true;
}

bool UEnclaveInventoryComponent::RemoveItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Item))
	{
		return false;
	}

	if (Items.Remove(Item) > 0)
	{
		ItemSourceChangedDelegate.Broadcast();
		return true;
	}

	return false;
}

bool UEnclaveInventoryComponent::ContainsItem(const AEnclaveItem* Item) const
{
	return IsValid(Item) && Items.Contains(Item);
}

AEnclaveItem* UEnclaveInventoryComponent::GetItemAt(int32 Index) const
{
	return Items.IsValidIndex(Index) ? Items[Index] : nullptr;
}

void UEnclaveInventoryComponent::OnRep_Items()
{
	ItemSourceChangedDelegate.Broadcast();
}
