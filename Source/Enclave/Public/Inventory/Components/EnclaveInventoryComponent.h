// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnclaveInventoryComponent.generated.h"

class AEnclaveItem;

/**
 * Manages the collection of items carried by the owning actor.
 *
 * Inventory owns carried-item membership and spawning. It does not determine
 * which item is currently equipped or how an item is physically presented.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveInventoryComponent : public UActorComponent
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

private:
	UPROPERTY(Replicated)
	TArray<TObjectPtr<AEnclaveItem>> Items;
};
