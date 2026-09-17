// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Equipment/Interfaces/EnclaveItemWielderInterface.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "Equipment/Items/EnclaveItem.h"

#include "Net/UnrealNetwork.h"
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

void UEnclaveEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentEquippedItem);
}

bool UEnclaveEquipmentComponent::EquipItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Item))
	{
		return false;
	}

	if (CurrentEquippedItem == Item)
	{
		return true;
	}

	if (IsValid(CurrentEquippedItem))
	{
		return false;
	}

	if (!AttachItemToOwner(Item))
	{
		return false;
	}

	CurrentEquippedItem = Item;

	return true;
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
		return HandleNextWieldableInput(InputEvent);
	}

	if (PreviousEquipmentInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PreviousEquipmentInputTag))
	{
		return HandlePreviousWieldableInput(InputEvent);
	}

	if (ItemActionPrimaryInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemActionPrimaryInputTag))
	{
		return HandleItemPrimaryActionInput(InputEvent);
	}

	if (ItemActionSecondaryInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemActionSecondaryInputTag))
	{
		return HandleItemSecondaryActionInput(InputEvent);
	}

	if (ItemActionTertiaryInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemActionTertiaryInputTag))
	{
		return HandleItemTertiaryActionInput(InputEvent);
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
	if (!IsValid(CurrentEquippedItem) || !CurrentEquippedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandlePrimaryAction(CurrentEquippedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformSecondaryAction(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentEquippedItem) || !CurrentEquippedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleSecondaryAction(CurrentEquippedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformTertiaryAction(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentEquippedItem) || !CurrentEquippedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleTertiaryAction(CurrentEquippedItem, InputEvent);
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

	if (ItemActionPrimaryInputTag.IsValid())
	{
		InputFilters.AddTag(ItemActionPrimaryInputTag);
	}

	if (ItemActionSecondaryInputTag.IsValid())
	{
		InputFilters.AddTag(ItemActionSecondaryInputTag);
	}

	if (ItemActionTertiaryInputTag.IsValid())
	{
		InputFilters.AddTag(ItemActionTertiaryInputTag);
	}
}

bool UEnclaveEquipmentComponent::HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryEquipNext();
}

bool UEnclaveEquipmentComponent::HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryEquipPrevious();
}

bool UEnclaveEquipmentComponent::HandleItemPrimaryActionInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformPrimaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleItemSecondaryActionInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformSecondaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleItemTertiaryActionInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformTertiaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::AttachItemToOwner(AEnclaveItem* Item)
{
	if (!IsValid(Item))
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return false;
	}

	IEnclaveItemWielderInterface* Wielder = Cast<IEnclaveItemWielderInterface>(OwnerActor);

	if (!Wielder)
	{
		return false;
	}

	const FGameplayTag ItemType = Item->GetItemType();

	if (!ItemType.IsValid())
	{
		return false;
	}

	FEnclaveWieldedItemAttachmentTargets AttachmentTargets;

	if (!Wielder->ResolveWieldedItemAttachmentTargets(ItemType, AttachmentTargets))
	{
		return false;
	}

	if (!IsValid(AttachmentTargets.FirstPerson.ParentComponent) ||
		!IsValid(AttachmentTargets.ThirdPerson.ParentComponent))
	{
		return false;
	}

	Item->AttachToActor(OwnerActor, FAttachmentTransformRules::KeepWorldTransform);

	const bool bFirstPersonAttached = Item->GetFirstPersonMesh()->AttachToComponent(
		AttachmentTargets.FirstPerson.ParentComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachmentTargets.FirstPerson.SocketName);

	const bool bThirdPersonAttached = Item->GetThirdPersonMesh()->AttachToComponent(
		AttachmentTargets.ThirdPerson.ParentComponent,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachmentTargets.ThirdPerson.SocketName);

	return bFirstPersonAttached && bThirdPersonAttached;
}

void UEnclaveEquipmentComponent::OnRep_CurrentEquippedItem()
{
	if (IsValid(CurrentEquippedItem))
	{
		AttachItemToOwner(CurrentEquippedItem);
	}
}
