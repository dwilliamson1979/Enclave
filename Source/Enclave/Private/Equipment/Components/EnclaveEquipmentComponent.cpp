// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "InputCoreTypes.h"

UEnclaveEquipmentComponent::UEnclaveEquipmentComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveEquipmentComponent::InitializeComponent()
{
	Super::InitializeComponent();

	RebuildInputFilters();
}

const FGameplayTagContainer& UEnclaveEquipmentComponent::GetInputFilters() const
{
	return InputFilters;
}

bool UEnclaveEquipmentComponent::HandleInput(const FEnclaveInputEvent& InputEvent)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			FString::Printf(TEXT("UEnclaveEquipmentComponent::HandleInput: %s"), *InputEvent.InputTag.ToString()));
	}

	if (NextEquipmentInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(NextEquipmentInputTag))
	{
		return HandleNextEquipmentInput(InputEvent);
	}

	if (PreviousEquipmentInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PreviousEquipmentInputTag))
	{
		return HandlePreviousEquipmentInput(InputEvent);
	}

	if (PrimaryItemInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PrimaryItemInputTag))
	{
		return HandlePrimaryItemInput(InputEvent);
	}

	if (SecondaryItemInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(SecondaryItemInputTag))
	{
		return HandleSecondaryItemInput(InputEvent);
	}

	if (TertiaryItemInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(TertiaryItemInputTag))
	{
		return HandleTertiaryItemInput(InputEvent);
	}

	return false;
}

bool UEnclaveEquipmentComponent::TryEquipNext()
{
	if (!CanCycleEquipment())
	{
		return false;
	}

	// TODO: Select the next valid active equipment slot.

	return true;
}

bool UEnclaveEquipmentComponent::TryEquipPrevious()
{
	if (!CanCycleEquipment())
	{
		return false;
	}

	// TODO: Select the previous valid active equipment slot.

	return true;
}

bool UEnclaveEquipmentComponent::CanCycleEquipment() const
{
	// TODO: Add equipment-state validation once active slots and
	// equipment transitions are implemented.

	return true;
}

bool UEnclaveEquipmentComponent::TryPerformPrimaryAction(const FEnclaveInputEvent& InputEvent)
{
	// TODO: Forward the complete input event to the currently equipped item.
	//
	// Example:
	// return CurrentEquippedItem
	//     ? CurrentEquippedItem->HandlePrimaryAction(InputEvent)
	//     : false;

	return false;
}

bool UEnclaveEquipmentComponent::TryPerformSecondaryAction(const FEnclaveInputEvent& InputEvent)
{
	// TODO: Forward the complete input event to the currently equipped item.

	return false;
}

bool UEnclaveEquipmentComponent::TryPerformTertiaryAction(const FEnclaveInputEvent& InputEvent)
{
	// TODO: Forward the complete input event to the currently equipped item.

	return false;
}

void UEnclaveEquipmentComponent::RebuildInputFilters()
{
	InputFilters.Reset();

	if (NextEquipmentInputTag.IsValid())
	{
		InputFilters.AddTag(NextEquipmentInputTag);
	}

	if (PreviousEquipmentInputTag.IsValid())
	{
		InputFilters.AddTag(PreviousEquipmentInputTag);
	}

	if (PrimaryItemInputTag.IsValid())
	{
		InputFilters.AddTag(PrimaryItemInputTag);
	}

	if (SecondaryItemInputTag.IsValid())
	{
		InputFilters.AddTag(SecondaryItemInputTag);
	}

	if (TertiaryItemInputTag.IsValid())
	{
		InputFilters.AddTag(TertiaryItemInputTag);
	}
}

bool UEnclaveEquipmentComponent::HandleNextEquipmentInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryEquipNext();
}

bool UEnclaveEquipmentComponent::HandlePreviousEquipmentInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryEquipPrevious();
}

bool UEnclaveEquipmentComponent::HandlePrimaryItemInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformPrimaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleSecondaryItemInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformSecondaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleTertiaryItemInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformTertiaryAction(InputEvent);
}