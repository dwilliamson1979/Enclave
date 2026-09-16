// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "EnclaveEquipmentComponent.generated.h"

class AEnclaveItem;

/**
 * Manages the character's active equipment and item-action input.
 *
 * This component owns operations that affect which item is currently equipped,
 * such as selecting the next or previous active equipment slot.
 *
 * It also routes generic item actions (Primary, Secondary, Tertiary) to the
 * currently equipped item.
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

	AEnclaveItem* SpawnDefaultItem(TSubclassOf<AEnclaveItem> ItemClass);

	AEnclaveItem* GetCurrentEquippedItem() const { return CurrentEquippedItem; }

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	bool TryEquipNext();
	bool TryEquipPrevious();
	bool CanCycleEquipment() const;

	bool TryPerformPrimaryAction(const FEnclaveInputEvent& InputEvent);
	bool TryPerformSecondaryAction(const FEnclaveInputEvent& InputEvent);
	bool TryPerformTertiaryAction(const FEnclaveInputEvent& InputEvent);

private:
	void RebuildInputFilters();

	bool HandleNextEquipmentInput(const FEnclaveInputEvent& InputEvent);
	bool HandlePreviousEquipmentInput(const FEnclaveInputEvent& InputEvent);

	bool HandlePrimaryItemInput(const FEnclaveInputEvent& InputEvent);
	bool HandleSecondaryItemInput(const FEnclaveInputEvent& InputEvent);
	bool HandleTertiaryItemInput(const FEnclaveInputEvent& InputEvent);

	/** Semantic input tag used to request the next active equipment item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag NextEquipmentInputTag;

	/** Semantic input tag used to request the previous active equipment item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag PreviousEquipmentInputTag;

	/** Semantic input tag used for the currently equipped item's primary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag PrimaryItemInputTag;

	/** Semantic input tag used for the currently equipped item's secondary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag SecondaryItemInputTag;

	/** Semantic input tag used for the currently equipped item's tertiary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag TertiaryItemInputTag;

	/** Cached routing filters derived from the configured input tags. */
	FGameplayTagContainer InputFilters;


	bool AttachItemToOwner(AEnclaveItem* Item);

	UFUNCTION()
	void OnRep_CurrentEquippedItem();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentEquippedItem)
	TObjectPtr<AEnclaveItem> CurrentEquippedItem = nullptr;
};