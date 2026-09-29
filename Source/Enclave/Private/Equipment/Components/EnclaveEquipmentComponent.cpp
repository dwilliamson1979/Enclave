// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayTags/EnclavePresentationTags.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

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

	// Authority owns the gameplay transition. Presentation is notified only
	// after the authoritative equipment state has been committed and cannot
	// reject or roll it back.
	RemoveGrantedAbilities();
	CurrentWieldedItem = Item;
	GrantAbilitiesForItem(CurrentWieldedItem);

	UpdateWieldedItemPresentation(PreviousItem, CurrentWieldedItem);
	LocallyWieldedItem = CurrentWieldedItem;

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

	if (ItemReloadActionInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ItemReloadActionInputTag))
	{
		return HandleItemReloadInput(InputEvent);
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
	const FGameplayAbilitySpecHandle SpecHandle = FindWieldedItemAbilityForInput(ItemPrimaryActionInputTag);
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!SpecHandle.IsValid() || !IsValid(ASC))
	{
		return false;
	}

	return ASC->TryActivateAbility(SpecHandle);
}

bool UEnclaveEquipmentComponent::TryPerformItemSecondary(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentWieldedItem))
	{
		return false;
	}

	// An ability the item bound to this input wins; only items that granted
	// nothing for it fall back to the wieldable interface. Ability
	// activation only on Started - secondary input also routes Triggered/
	// Completed events for the interface path (hold-to-aim), and those must
	// not re-activate an ability every frame.
	const FGameplayAbilitySpecHandle SpecHandle = FindWieldedItemAbilityForInput(ItemSecondaryActionInputTag);

	if (SpecHandle.IsValid())
	{
		UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
		return InputEvent.TriggerEvent == ETriggerEvent::Started && IsValid(ASC) && ASC->TryActivateAbility(SpecHandle);
	}

	if (!CurrentWieldedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleSecondaryAction(CurrentWieldedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformItemTertiary(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentWieldedItem))
	{
		return false;
	}

	// Same ability-first, interface-fallback rule as TryPerformItemSecondary.
	const FGameplayAbilitySpecHandle SpecHandle = FindWieldedItemAbilityForInput(ItemTertiaryActionInputTag);

	if (SpecHandle.IsValid())
	{
		UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
		return InputEvent.TriggerEvent == ETriggerEvent::Started && IsValid(ASC) && ASC->TryActivateAbility(SpecHandle);
	}

	if (!CurrentWieldedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleTertiaryAction(CurrentWieldedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformItemReload(const FEnclaveInputEvent& InputEvent)
{
	const FGameplayAbilitySpecHandle SpecHandle = FindWieldedItemAbilityForInput(ItemReloadActionInputTag);
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!SpecHandle.IsValid() || !IsValid(ASC))
	{
		return false;
	}

	// Whether a reload makes sense right now (magazine already full, etc.)
	// is UEnclaveGA_Reload::CanActivateAbility's call, not this component's.
	return ASC->TryActivateAbility(SpecHandle);
}

FGameplayAbilitySpecHandle UEnclaveEquipmentComponent::FindWieldedItemAbilityForInput(const FGameplayTag& InputTag) const
{
	if (!InputTag.IsValid() || !IsValid(CurrentWieldedItem))
	{
		return FGameplayAbilitySpecHandle();
	}

	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return FGameplayAbilitySpecHandle();
	}

	// Replaces the Phase 2 "GrantedClasses[0] is the primary action"
	// shortcut. Reads the ASC's activatable specs rather than
	// GrantedAbilityHandles: that array is server-only bookkeeping (filled
	// only where WieldItem's authority check passes), while the specs
	// themselves - SourceObject and dynamic source tags included -
	// replicate to the owning client, so this resolves identically on both
	// sides for LocalPredicted activation. Matching on SourceObject as well
	// as the tag keeps a stale spec from a just-swapped-out weapon (removal
	// not yet replicated) from ever answering for the new one.
	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.SourceObject.Get() == CurrentWieldedItem.Get() && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return Spec.Handle;
		}
	}

	return FGameplayAbilitySpecHandle();
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

	if (ItemReloadActionInputTag.IsValid())
	{
		InputFilters.AddTag(ItemReloadActionInputTag);
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
	//Hard-coded for primary fire at the moment, but this will be updated to support other trigger events in the future: charged, auto, burst.
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

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

bool UEnclaveEquipmentComponent::HandleItemReloadInput(const FEnclaveInputEvent& InputEvent)
{	
	// Press-to-reload. A future "hold to inspect / tap to reload" split
	// would branch on TriggerEvent here.
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryPerformItemReload(InputEvent);
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
		AEnclaveItem* PreviousItem = CurrentWieldedItem;
		RemoveGrantedAbilities();
		CurrentWieldedItem = nullptr;
		UpdateWieldedItemPresentation(PreviousItem, nullptr);
		LocallyWieldedItem.Reset();
	}
}

IEnclaveAvatarPresentationInterface* UEnclaveEquipmentComponent::GetAvatarPresentation() const
{
	return IEnclaveAvatarPresentationInterface::FindAvatarPresentation(GetOwner());
}

void UEnclaveEquipmentComponent::UpdateWieldedItemPresentation(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem)
{
	IEnclaveAvatarPresentationInterface* AvatarPresentation = GetAvatarPresentation();

	if (!AvatarPresentation)
	{
		return;
	}

	if (IsValid(PreviousItem) && PreviousItem != NewItem)
	{
		AvatarPresentation->ClearWieldedItemPresentation(PreviousItem);
	}

	if (IsValid(NewItem))
	{
		AvatarPresentation->PresentWieldedItem(NewItem);

		// Equipment owns the semantic wield transition. Once the new item's
		// persistent presentation is established, explicitly report the Draw
		// action rather than making AvatarPresentation infer it from state.
		if (PreviousItem != NewItem)
		{
			FEnclaveItemPresentationActionEvent DrawEvent;
			DrawEvent.ItemType = NewItem->GetItemType();
			DrawEvent.Action = EnclaveGameplayTags::Presentation_Item_Action_Draw;
			AvatarPresentation->HandleItemPresentationAction(DrawEvent);
		}
	}
}

void UEnclaveEquipmentComponent::ServerTryWieldNext_Implementation()
{
	TryWieldNext();
}

void UEnclaveEquipmentComponent::ServerTryWieldPrevious_Implementation()
{
	TryWieldPrevious();
}

UAbilitySystemComponent* UEnclaveEquipmentComponent::GetOwnerAbilitySystemComponent() const
{
	const AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	// Deliberately goes through the interface rather than casting to
	// AEnclaveCharacter - Equipment doesn't need to know who hosts the ASC,
	// only that its owner exposes one (same principle as ItemSource above).
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(OwnerActor);
	return AbilitySystemInterface ? AbilitySystemInterface->GetAbilitySystemComponent() : nullptr;
}

void UEnclaveEquipmentComponent::GrantAbilitiesForItem(AEnclaveItem* Item)
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!IsValid(ASC) || !IsValid(Item))
	{
		return;
	}

	for (const FEnclaveAbilityGrant& Grant : Item->GetAbilityGrants())
	{
		if (!Grant.AbilityClass)
		{
			continue;
		}

		// SourceObject = Item: lets the ability later identify which weapon
		// activated it (via GetCurrentAbilitySpec()->SourceObject) without
		// Equipment needing to pass that context through some other path.
		FGameplayAbilitySpec AbilitySpec(Grant.AbilityClass, /*Level=*/ 1, /*InputID=*/ INDEX_NONE, /*SourceObject=*/ Item);

		// The input binding rides on the spec itself (dynamic source tags
		// replicate with the spec), which is what
		// FindWieldedItemAbilityForInput matches against.
		if (Grant.InputTag.IsValid())
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
		}
		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(AbilitySpec);

		GrantedAbilityHandles.Add(Handle);
	}
}

void UEnclaveEquipmentComponent::RemoveGrantedAbilities()
{
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
		{
			ASC->ClearAbility(Handle);
		}
	}

	GrantedAbilityHandles.Reset();
}

void UEnclaveEquipmentComponent::OnRep_CurrentWieldedItem()
{
	AEnclaveItem* PreviousItem = LocallyWieldedItem.Get();

	UpdateWieldedItemPresentation(PreviousItem, CurrentWieldedItem);
	LocallyWieldedItem = CurrentWieldedItem;
}
