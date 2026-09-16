// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Components/SkeletalMeshComponent.h"

#include "Characters/EnclaveCharacter.h"

#include "Equipment/Interfaces/EnclaveItemWielderInterface.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "Equipment/Items/EnclaveItem.h"

#include "GameFramework/Pawn.h"

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

AEnclaveItem* UEnclaveEquipmentComponent::SpawnDefaultItem(TSubclassOf<AEnclaveItem> ItemClass)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !ItemClass)
	{
		return nullptr;
	}

	if (IsValid(CurrentEquippedItem))
	{
		return CurrentEquippedItem;
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

	CurrentEquippedItem = Item;

	AttachItemToOwner(Item);

	return Item;
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
		FAttachmentTransformRules::KeepRelativeTransform,// SnapToTargetNotIncludingScale,
		AttachmentTargets.FirstPerson.SocketName);

	const bool bThirdPersonAttached = Item->GetThirdPersonMesh()->AttachToComponent(
		AttachmentTargets.ThirdPerson.ParentComponent,
		FAttachmentTransformRules::KeepRelativeTransform,// SnapToTargetNotIncludingScale,
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