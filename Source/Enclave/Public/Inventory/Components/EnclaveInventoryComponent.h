// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inventory/Interfaces/EnclaveItemSourceInterface.h"
#include "EnclaveInventoryComponent.generated.h"

class AEnclaveItem;

/**
 * Manages the collection of items carried by the owning actor.
 *
 * Inventory owns carried-item membership and spawning. It does not determine
 * which item is currently equipped or how an item is physically presented.
 *
 * Exposes IEnclaveItemSourceInterface so consumers (e.g. EquipmentComponent)
 * can read and subscribe to changes in carried items without depending on
 * this concrete class.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveInventoryComponent : public UActorComponent, public IEnclaveItemSourceInterface
{
	GENERATED_BODY()

public:
	UEnclaveInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	AEnclaveItem* SpawnItem(TSubclassOf<AEnclaveItem> ItemClass);

	bool AddItem(AEnclaveItem* Item);
	bool RemoveItem(AEnclaveItem* Item);
	bool ContainsItem(const AEnclaveItem* Item) const;

	bool IsEmpty() const { return Items.IsEmpty(); }
	int32 GetItemCount() const { return Items.Num(); }
	const TArray<TObjectPtr<AEnclaveItem>>& GetItems() const { return Items; }
	AEnclaveItem* GetItemAt(int32 Index) const;

	// IEnclaveItemSourceInterface
	virtual const TArray<TObjectPtr<AEnclaveItem>>& GetSourceItems() const override { return Items; }
	virtual bool ContainsSourceItem(const AEnclaveItem* Item) const override { return ContainsItem(Item); }
	virtual FOnEnclaveItemSourceChanged& OnItemSourceChanged() override { return ItemSourceChangedDelegate; }

private:
	UFUNCTION()
	void OnRep_Items();

	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<TObjectPtr<AEnclaveItem>> Items;

	/** Broadcast whenever Items changes: locally on add/remove, and on OnRep for remote machines. */
	FOnEnclaveItemSourceChanged ItemSourceChangedDelegate;
};
