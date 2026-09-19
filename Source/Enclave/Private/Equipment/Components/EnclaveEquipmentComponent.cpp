// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Equipment/Interfaces/EnclaveItemWielderInterface.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "Equipment/Items/EnclaveItem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"

UEnclaveEquipmentComponent::UEnclaveEquipmentComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveEquipmentComponent::InitializeComponent()
{
	Super::InitializeComponent();

	RebuildInputFilters();
	BindItemSource();
}

void UEnclaveEquipmentComponent::UninitializeComponent()
{
	UnbindItemSource();

	Super::UninitializeComponent();
}

void UEnclaveEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentWieldedItem);
}

bool UEnclaveEquipmentComponent::WieldItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsAvailableWieldable(Item))
	{
		return false;
	}

	if (CurrentWieldedItem == Item)
	{
		return true;
	}

	AEnclaveItem* PreviousItem = CurrentWieldedItem;

	if (IsValid(PreviousItem))
	{
		SetWieldedPresentation(PreviousItem, false);
	}

	if (!AttachItemToOwner(Item))
	{
		if (IsValid(PreviousItem) && AttachItemToOwner(PreviousItem))
		{
			SetWieldedPresentation(PreviousItem, true);
			PresentedWieldedItem = PreviousItem;
		}

		return false;
	}

	CurrentWieldedItem = Item;
	SetWieldedPresentation(CurrentWieldedItem, true);
	PresentedWieldedItem = CurrentWieldedItem;

	return true;
}

const FGameplayTagContainer& UEnclaveEquipmentComponent::GetInputFilters() const
{
	return InputFilters;
}

bool UEnclaveEquipmentComponent::HandleInput(const FEnclaveInputEvent& InputEvent)
{
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
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		ServerTryWieldNext();
		return true;
	}

	return TryWieldInDirection(1);
}

bool UEnclaveEquipmentComponent::TryWieldPrevious()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return false;
	}

	if (!OwnerActor->HasAuthority())
	{
		ServerTryWieldPrevious();
		return true;
	}

	return TryWieldInDirection(-1);
}

bool UEnclaveEquipmentComponent::CanCycleWieldables() const
{
	return IsValid(CurrentWieldedItem) ? WieldableCache.Num() > 1 : WieldableCache.Num() > 0;
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

bool UEnclaveEquipmentComponent::TryWieldInDirection(int32 Direction)
{
	if (Direction != 1 && Direction != -1)
	{
		return false;
	}

	if (!CanCycleWieldables())
	{
		return false;
	}

	const int32 ItemCount = WieldableCache.Num();

	if (ItemCount == 0)
	{
		return false;
	}

	int32 CurrentIndex = WieldableCache.IndexOfByPredicate([this](const TObjectPtr<AEnclaveItem>& Item)
	{
		return Item == CurrentWieldedItem;
	});

	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = Direction > 0 ? -1 : 0;
	}

	for (int32 Offset = 0; Offset < ItemCount; ++Offset)
	{
		CurrentIndex = (CurrentIndex + Direction + ItemCount) % ItemCount;
		AEnclaveItem* Candidate = WieldableCache[CurrentIndex];

		if (!IsValid(Candidate) || Candidate == CurrentWieldedItem)
		{
			continue;
		}

		return WieldItem(Candidate);
	}

	return false;
}

bool UEnclaveEquipmentComponent::IsAvailableWieldable(const AEnclaveItem* Item) const
{
	return IsValid(Item) && WieldableCache.Contains(Item);
}

void UEnclaveEquipmentComponent::BindItemSource()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return;
	}

	UActorComponent* SourceComponent = OwnerActor->FindComponentByInterface(UEnclaveItemSourceInterface::StaticClass());

	if (!IsValid(SourceComponent))
	{
		return;
	}

	IEnclaveItemSourceInterface* SourceInterface = Cast<IEnclaveItemSourceInterface>(SourceComponent);

	if (!SourceInterface)
	{
		return;
	}

	ItemSource.SetObject(SourceComponent);
	ItemSource.SetInterface(SourceInterface);

	ItemSource->OnItemSourceChanged().AddUObject(this, &UEnclaveEquipmentComponent::HandleItemSourceChanged);
	RebuildWieldableCache();
}

void UEnclaveEquipmentComponent::UnbindItemSource()
{
	if (ItemSource)
	{
		ItemSource->OnItemSourceChanged().RemoveAll(this);
	}

	ItemSource.SetInterface(nullptr);
	ItemSource.SetObject(nullptr);
	WieldableCache.Reset();
}

void UEnclaveEquipmentComponent::HandleItemSourceChanged()
{
	RebuildWieldableCache();
}

void UEnclaveEquipmentComponent::RebuildWieldableCache()
{
	WieldableCache.Reset();

	if (!ItemSource)
	{
		return;
	}

	for (const TObjectPtr<AEnclaveItem>& Item : ItemSource->GetSourceItems())
	{
		if (IsValid(Item) && Item->Implements<UEnclaveWieldableInterface>())
		{
			WieldableCache.Add(Item);
		}
	}

	// If the currently wielded item was removed from the source, only authority
	// may correct replicated wielded state. Clients rebuild their local cache
	// from OnRep_Items and wait for CurrentWieldedItem replication rather than
	// mutating an authoritative replicated property locally.
	AActor* OwnerActor = GetOwner();

	if (IsValid(OwnerActor) && OwnerActor->HasAuthority() &&
		IsValid(CurrentWieldedItem) && !WieldableCache.Contains(CurrentWieldedItem))
	{
		SetWieldedPresentation(CurrentWieldedItem, false);
		CurrentWieldedItem = nullptr;
		PresentedWieldedItem.Reset();
	}
}

bool UEnclaveEquipmentComponent::AttachItemToOwner(AEnclaveItem* Item)
{
	if (!IsValid(Item) || !IsValid(Item->GetFirstPersonMesh()) || !IsValid(Item->GetThirdPersonMesh()))
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

void UEnclaveEquipmentComponent::SetWieldedPresentation(AEnclaveItem* Item, bool bWielded)
{
	if (!IsValid(Item))
	{
		return;
	}

	Item->SetActorHiddenInGame(!bWielded);
}

void UEnclaveEquipmentComponent::ServerTryWieldNext_Implementation()
{
	TryWieldNext();
}

void UEnclaveEquipmentComponent::ServerTryWieldPrevious_Implementation()
{
	TryWieldPrevious();
}

void UEnclaveEquipmentComponent::OnRep_CurrentWieldedItem()
{
	if (AEnclaveItem* PreviousPresentedItem = PresentedWieldedItem.Get();
		IsValid(PreviousPresentedItem) && PreviousPresentedItem != CurrentWieldedItem)
	{
		SetWieldedPresentation(PreviousPresentedItem, false);
	}

	if (IsValid(CurrentWieldedItem) && AttachItemToOwner(CurrentWieldedItem))
	{
		SetWieldedPresentation(CurrentWieldedItem, true);
		PresentedWieldedItem = CurrentWieldedItem;
	}
	else
	{
		PresentedWieldedItem.Reset();
	}
}
