// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "EnclaveEquipmentComponent.generated.h"

class AEnclaveItem;
class UEnclaveInventoryComponent;

/**
 * Manages the character's currently wielded item and item-action input.
 *
 * This component owns wielding operations, including selecting the next or
 * previous wieldable item from inventory and presenting the active item.
 *
 * It also routes generic item actions (Primary, Secondary, Tertiary) to the
 * currently wielded item.
 *
 * Inventory/storage responsibilities do not belong here.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveEquipmentComponent : public UActorComponent, public IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:
	UEnclaveEquipmentComponent();

	// UActorComponent
	virtual void InitializeComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool WieldItem(AEnclaveItem* Item);
	AEnclaveItem* GetCurrentWieldedItem() const { return CurrentWieldedItem; }

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	bool TryWieldNext();
	bool TryWieldPrevious();
	bool CanCycleWieldables() const;

	bool TryPerformItemPrimary(const FEnclaveInputEvent& InputEvent);
	bool TryPerformItemSecondary(const FEnclaveInputEvent& InputEvent);
	bool TryPerformItemTertiary(const FEnclaveInputEvent& InputEvent);

private:
	void RebuildInputFilters();

	bool HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent);

	bool HandleItemPrimaryInput(const FEnclaveInputEvent& InputEvent);
	bool HandleItemSecondaryInput(const FEnclaveInputEvent& InputEvent);
	bool HandleItemTertiaryInput(const FEnclaveInputEvent& InputEvent);

	bool TryWieldInDirection(int32 Direction);
	bool IsWieldableInventoryItem(const AEnclaveItem* Item) const;
	UEnclaveInventoryComponent* GetInventoryComponent() const;

	bool AttachItemToOwner(AEnclaveItem* Item);
	void SetWieldedPresentation(AEnclaveItem* Item, bool bWielded);

	UFUNCTION(Server, Reliable)
	void ServerTryWieldNext();

	UFUNCTION(Server, Reliable)
	void ServerTryWieldPrevious();

	/** Semantic input tag used to request the next wieldable inventory item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag NextWieldableInputTag;

	/** Semantic input tag used to request the previous wieldable inventory item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag PreviousWieldableInputTag;

	/** Semantic input tag used for the currently wielded item's primary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag ItemPrimaryActionInputTag;

	/** Semantic input tag used for the currently wielded item's secondary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag ItemSecondaryActionInputTag;

	/** Semantic input tag used for the currently wielded item's tertiary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag ItemTertiaryActionInputTag;

	/** Cached routing filters derived from the configured input tags. */
	FGameplayTagContainer InputFilters;

	UFUNCTION()
	void OnRep_CurrentWieldedItem();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWieldedItem)
	TObjectPtr<AEnclaveItem> CurrentWieldedItem = nullptr;

	/** Local presentation cache used to clean up the previously wielded item on replication. */
	TWeakObjectPtr<AEnclaveItem> PresentedWieldedItem;
};
