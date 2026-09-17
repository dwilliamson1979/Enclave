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

	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentWieldedItem);
}

bool UEnclaveEquipmentComponent::WieldItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Item))
	{
		return false;
	}

	if (CurrentWieldedItem == Item)
	{
		return true;
	}

	if (IsValid(CurrentWieldedItem))
	{
		return false;
	}

	if (!AttachItemToOwner(Item))
	{
		return false;
	}

	CurrentWieldedItem = Item;

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

	if (NextWieldableInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(NextWieldableInputTag))
	{
		return HandleNextWieldableInput(InputEvent);
	}

	if (PreviousWieldableInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PreviousWieldableInputTag))
	{
		return HandlePreviousWieldableInput(InputEvent);
	}

	if (ItemPrimaryActionInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemPrimaryActionInputTag))
	{
		return HandleItemPrimaryInput(InputEvent);
	}

	if (ItemSecondaryActionInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemSecondaryActionInputTag))
	{
		return HandleItemSecondaryInput(InputEvent);
	}

	if (ItemTertiaryActionInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemTertiaryActionInputTag))
	{
		return HandleItemTertiaryInput(InputEvent);
	}

	return false;
}

bool UEnclaveEquipmentComponent::TryWieldNext()
{
	if (!CanCycleWieldables())
	{
		return false;
	}

	// TODO: Select the next valid active equipment slot.

	return true;
}

bool UEnclaveEquipmentComponent::TryWieldPrevious()
{
	if (!CanCycleWieldables())
	{
		return false;
	}

	// TODO: Select the previous valid active equipment slot.

	return true;
}

bool UEnclaveEquipmentComponent::CanCycleWieldables() const
{
	// TODO: Add equipment-state validation once active slots and
	// equipment transitions are implemented.

	return true;
}

bool UEnclaveEquipmentComponent::TryPerformItemPrimary(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentWieldedItem) || !CurrentWieldedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandlePrimaryAction(CurrentWieldedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformItemSecondary(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentWieldedItem) || !CurrentWieldedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleSecondaryAction(CurrentWieldedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformItemTertiary(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentWieldedItem) || !CurrentWieldedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleTertiaryAction(CurrentWieldedItem, InputEvent);
}

void UEnclaveEquipmentComponent::RebuildInputFilters()
{
	InputFilters.Reset();

	if (NextWieldableInputTag.IsValid())
	{
		InputFilters.AddTag(NextWieldableInputTag);
	}

	if (PreviousWieldableInputTag.IsValid())
	{
		InputFilters.AddTag(PreviousWieldableInputTag);
	}

	if (ItemPrimaryActionInputTag.IsValid())
	{
		InputFilters.AddTag(ItemPrimaryActionInputTag);
	}

	if (ItemSecondaryActionInputTag.IsValid())
	{
		InputFilters.AddTag(ItemSecondaryActionInputTag);
	}

	if (ItemTertiaryActionInputTag.IsValid())
	{
		InputFilters.AddTag(ItemTertiaryActionInputTag);
	}
}

bool UEnclaveEquipmentComponent::HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryWieldNext();
}

bool UEnclaveEquipmentComponent::HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryWieldPrevious();
}

bool UEnclaveEquipmentComponent::HandleItemPrimaryInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformItemPrimary(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleItemSecondaryInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformItemSecondary(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleItemTertiaryInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformItemTertiary(InputEvent);
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

void UEnclaveEquipmentComponent::OnRep_CurrentWieldedItem()
{
	if (IsValid(CurrentWieldedItem))
	{
		AttachItemToOwner(CurrentWieldedItem);
	}
}
