// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "EnclaveEquipmentComponent.generated.h"

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

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	/**
	 * Attempts to select the next available equipment slot.
	 *
	 * Returns true if the request was accepted.
	 */
	bool TryEquipNext();

	/**
	 * Attempts to select the previous available equipment slot.
	 *
	 * Returns true if the request was accepted.
	 */
	bool TryEquipPrevious();

	/**
	 * Returns whether equipment cycling is currently allowed.
	 */
	bool CanCycleEquipment() const;

	/**
	 * Attempts to perform the currently equipped item's primary action.
	 *
	 * The complete input event is preserved so the equipped item can respond
	 * to the input phases appropriate for its behavior.
	 */
	bool TryPerformPrimaryAction(const FEnclaveInputEvent& InputEvent);

	/**
	 * Attempts to perform the currently equipped item's secondary action.
	 */
	bool TryPerformSecondaryAction(const FEnclaveInputEvent& InputEvent);

	/**
	 * Attempts to perform the currently equipped item's tertiary action.
	 */
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
};