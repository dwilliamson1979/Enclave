// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Abilities/Effects/EnclaveGE_ItemEquipping.h"
#include "Debug/EnclaveDebug.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Equipment/Data/EnclaveItemDefinition.h"
#include "GameplayTags/EnclaveInputTags.h"
#include "GameplayTags/EnclaveItemTags.h"
#include "GameplayTags/EnclavePresentationTags.h"
#include "GameplayTags/EnclaveAbilityTags.h"
#include "GameplayTags/EnclaveEquipmentSlotTags.h"

#include "AbilitySystemComponent.h"
#include "Abilities/EnclaveAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "GameplayTags/EnclaveEffectTags.h"
#include "Abilities/Effects/EnclaveGameplayEffect.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UEnclaveEquipmentComponent::UEnclaveEquipmentComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;

	DefaultSelectionSlotTag = EnclaveGameplayTags::Equipment_Slot_Default;
	ToggleStowedInputTag = EnclaveGameplayTags::Input_Equipment_Stowed_Toggle;
	// Only ordinary slots belong in the editable array. The reserved identities
	// resolve through FindSlotDefinition and share the same assignment storage.
	const FGameplayTag NativeSlotTags[] = {
		EnclaveGameplayTags::Equipment_Slot_Primary,
		EnclaveGameplayTags::Equipment_Slot_Secondary,
		EnclaveGameplayTags::Equipment_Slot_Tertiary
	};
	for (const FGameplayTag& Tag : NativeSlotTags)
	{
		FEnclaveEquipmentSlotDefinition& Slot = SlotDefinitions.AddDefaulted_GetRef();
		Slot.SlotTag = Tag;
		Slot.bCanWield = true;
		Slot.bInWieldCycle = true;
		Slot.RequiredCapabilities.AddTag(EnclaveGameplayTags::Item_Capability_Wieldable);
	}

	DefaultReservedDefinition.SlotTag = EnclaveGameplayTags::Equipment_Slot_Default;
	DefaultReservedDefinition.bCanWield = true;
	DefaultReservedDefinition.bInWieldCycle = true;
	DefaultReservedDefinition.RequiredCapabilities.AddTag(EnclaveGameplayTags::Item_Capability_Wieldable);

	StowedReservedDefinition.SlotTag = EnclaveGameplayTags::Equipment_Slot_Stowed;
	StowedReservedDefinition.bCanWield = true;
	StowedReservedDefinition.bInWieldCycle = false;
	StowedReservedDefinition.RequiredCapabilities.AddTag(EnclaveGameplayTags::Item_Capability_Wieldable);
}

void UEnclaveEquipmentComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// Preserve authored restrictions from pre-refactor serialized Blueprint rows.
	// The rows are no longer ordinary slots; their reserved identities and Stowed
	// selection policy cannot be overridden. Authors should migrate then remove them.
	for (const FEnclaveEquipmentSlotDefinition& Legacy : SlotDefinitions)
	{
		if (Legacy.SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default))
		{
			DefaultReservedDefinition.bInWieldCycle = bDefaultInWieldCycle && Legacy.bInWieldCycle;
			DefaultReservedDefinition.RequiredCapabilities.AppendTags(Legacy.RequiredCapabilities);
		}
		else if (Legacy.SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
		{
			StowedReservedDefinition.RequiredCapabilities.AppendTags(Legacy.RequiredCapabilities);
		}
	}
	if (!SlotDefinitions.ContainsByPredicate([](const FEnclaveEquipmentSlotDefinition& D)
		{ return D.SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default); }))
	{
		DefaultReservedDefinition.bInWieldCycle = bDefaultInWieldCycle;
	}

	RebuildInputFilters();
	ValidateSlotDefinitions();
}

void UEnclaveEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	// ItemSource is assigned explicitly by the owner, never discovered here.
}

void UEnclaveEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEquipmentTearingDown = true;
	// EndPlay may precede UninitializeComponent. Release slot-owned GAS state
	// before the pawn and its PlayerState-owned ASC become separated.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		RemoveAllEquippedEffects();
	}
	Super::EndPlay(EndPlayReason);
}

void UEnclaveEquipmentComponent::UninitializeComponent()
{
	bEquipmentTearingDown = true;
	ReleaseHeldItemInputs();
	AActor* OwnerActor = GetOwner();

	// The ASC is PlayerState-owned and can outlive this Pawn. Revoke every
	// Pawn/item-scoped gameplay contribution before this component goes away so
	// a non-death teardown (travel, admin destruction, etc.) cannot leak specs or
	// an active equip effect into the next avatar.
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		ClearCurrentWieldedItem();
		RemoveAllEquippedEffects();
		ManagedAbilitySystem.Reset();
	}

	for (const FEnclaveEquipmentSlotAssignment& Assignment : SlotAssignments)
	{
		if (!Assignment.bFromItemSource && IsValid(Assignment.Item))
		{
			Assignment.Item->OnDestroyed.RemoveDynamic(this, &UEnclaveEquipmentComponent::HandleAssignedItemDestroyed);
		}
	}
	UnbindItemSource();

	Super::UninitializeComponent();
}

void UEnclaveEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentWieldedItem);
	DOREPLIFETIME(UEnclaveEquipmentComponent, SlotAssignments);
	DOREPLIFETIME(UEnclaveEquipmentComponent, SelectedSlotTag);
}

bool UEnclaveEquipmentComponent::SetItemSource(UActorComponent* SourceComponent)
{
	if (SourceComponent && (SourceComponent->GetOwner() != GetOwner() ||
		!SourceComponent->GetClass()->ImplementsInterface(UEnclaveItemSourceInterface::StaticClass())))
	{
		return false;
	}

	if (ItemSource.GetObject() == SourceComponent)
	{
		return true;
	}

	UnbindItemSource();

	if (SourceComponent)
	{
		IEnclaveItemSourceInterface* SourceInterface = Cast<IEnclaveItemSourceInterface>(SourceComponent);
		if (!SourceInterface)
		{
			return false;
		}
		ItemSource.SetObject(SourceComponent);
		ItemSource.SetInterface(SourceInterface);
		ItemSource->OnItemSourceChanged().AddUObject(this, &UEnclaveEquipmentComponent::HandleItemSourceChanged);
	}

	// The old source no longer authorizes any of its assigned items. External
	// assignments are independent and survive source replacement.
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		for (int32 Index = SlotAssignments.Num() - 1; Index >= 0; --Index)
		{
			if (SlotAssignments[Index].bFromItemSource)
			{
				SlotAssignments.RemoveAt(Index);
			}
		}
		RefreshEquippedEffects();
		ReconcileSelectionAfterSlotChange();
	}
	return true;
}

const FEnclaveEquipmentSlotDefinition* UEnclaveEquipmentComponent::FindSlotDefinition(FGameplayTag SlotTag) const
{
	// Reserved identities take precedence over stale serialized Blueprint rows.
	// Stowed's existence in the registry does not imply an assigned item.
	if (SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default))
	{
		return &DefaultReservedDefinition;
	}
	if (SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
	{
		return &StowedReservedDefinition;
	}
	return SlotDefinitions.FindByPredicate([SlotTag](const FEnclaveEquipmentSlotDefinition& Definition)
	{
		return Definition.SlotTag.MatchesTagExact(SlotTag);
	});
}

void UEnclaveEquipmentComponent::ValidateSlotDefinitions() const
{
	TSet<FGameplayTag> Seen;
	for (const FEnclaveEquipmentSlotDefinition& Definition : SlotDefinitions)
	{
		if (!Definition.SlotTag.IsValid())
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: ordinary slot has an invalid tag."), *GetNameSafe(GetOwner()));
			continue;
		}
		if (Definition.SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default) ||
			Definition.SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: legacy reserved slot %s in SlotDefinitions: capability restrictions (and Default cycle flag) are honored for migration, but remove the Blueprint row after configuring reserved slots."),
				*GetNameSafe(GetOwner()), *Definition.SlotTag.ToString());
			continue;
		}
		if (Seen.Contains(Definition.SlotTag))
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: duplicate ordinary slot %s. Only the first matching definition is used."),
				*GetNameSafe(GetOwner()), *Definition.SlotTag.ToString());
		}
		Seen.Add(Definition.SlotTag);
		if (Definition.bInWieldCycle && !Definition.bCanWield)
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: %s is cycle-enabled without bCanWield. Retaining legacy wield eligibility; enable bCanWield when migrating."),
				*GetNameSafe(GetOwner()), *Definition.SlotTag.ToString());
		}
	}
}

void UEnclaveEquipmentComponent::BuildCycleSlotTags(TArray<FGameplayTag>& OutSlotTags) const
{
	OutSlotTags.Reset();
	for (const FEnclaveEquipmentSlotDefinition& Definition : SlotDefinitions)
	{
		const FGameplayTag Tag = Definition.SlotTag;
		if (!Tag.IsValid() || Tag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default) ||
			Tag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed) ||
			!Definition.bInWieldCycle || OutSlotTags.Contains(Tag))
		{
			continue;
		}
		if (IsValid(GetItemInSlot(Tag)))
		{
			OutSlotTags.Add(Tag);
		}
	}
	// Preserve the original ordinary-slots-then-Default cycle order.
	if (DefaultReservedDefinition.bInWieldCycle && IsValid(GetItemInSlot(EnclaveGameplayTags::Equipment_Slot_Default)))
	{
		OutSlotTags.Add(EnclaveGameplayTags::Equipment_Slot_Default);
	}
}

const FEnclaveEquipmentSlotAssignment* UEnclaveEquipmentComponent::FindSlotAssignment(FGameplayTag SlotTag) const
{
	return SlotAssignments.FindByPredicate([SlotTag](const FEnclaveEquipmentSlotAssignment& Assignment)
	{
		return Assignment.SlotTag.MatchesTagExact(SlotTag);
	});
}

AEnclaveItem* UEnclaveEquipmentComponent::GetItemInSlot(FGameplayTag SlotTag) const
{
	const FEnclaveEquipmentSlotAssignment* Assignment = FindSlotAssignment(SlotTag);
	if (!Assignment || !IsValid(Assignment->Item))
	{
		return nullptr;
	}
	// Replicated source memberships can arrive after slot assignments. The
	// server checks live membership; clients consume the server's assignment.
	return GetOwner() && GetOwner()->HasAuthority() && !IsAssignmentValid(*Assignment)
		? nullptr : Assignment->Item.Get();
}

bool UEnclaveEquipmentComponent::IsAssignmentValid(const FEnclaveEquipmentSlotAssignment& Assignment) const
{
	AEnclaveItem* Item = Assignment.Item.Get();
	if (!IsValid(Item) || !FindSlotDefinition(Assignment.SlotTag))
	{
		return false;
	}
	return Assignment.bFromItemSource
		? (ItemSource && ItemSource->ContainsSourceItem(Item))
		: Item->GetOwner() == GetOwner();
}

bool UEnclaveEquipmentComponent::AssignSourceItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item)
{
	return AssignItemToSlot(SlotTag, Item, true);
}

bool UEnclaveEquipmentComponent::AssignExternalItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item)
{
	return AssignItemToSlot(SlotTag, Item, false);
}

bool UEnclaveEquipmentComponent::AssignItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item, bool bFromItemSource)
{
	AActor* OwnerActor = GetOwner();
	const FEnclaveEquipmentSlotDefinition* Definition = FindSlotDefinition(SlotTag);
	const bool bIsReservedSlot = SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default) ||
		SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed);
	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority() ||
		!IsValid(Item) || !Definition || (bIsReservedSlot && bFromItemSource))
	{
		return false;
	}

	// Only source membership or server-verified external ownership authorizes a
	// slot assignment. Never accept arbitrary world actors from client input.
	if (bFromItemSource ? !(ItemSource && ItemSource->ContainsSourceItem(Item))
		: (Item->GetOwner() != OwnerActor || (ItemSource && ItemSource->ContainsSourceItem(Item))))
	{
		return false;
	}

	if ((Definition->bCanWield || Definition->bInWieldCycle) && !Item->HasItemCapability(EnclaveGameplayTags::Item_Capability_Wieldable))
	{
		return false;
	}
	for (const FGameplayTag& Required : Definition->RequiredCapabilities)
	{
		if (!Item->HasItemCapability(Required))
		{
			return false;
		}
	}

	// An actor is assigned to one slot at a time.
	for (const FEnclaveEquipmentSlotAssignment& Assignment : SlotAssignments)
	{
		if (Assignment.Item == Item && !Assignment.SlotTag.MatchesTagExact(SlotTag))
		{
			return false;
		}
	}

	FEnclaveEquipmentSlotAssignment* Existing = SlotAssignments.FindByPredicate([SlotTag](const FEnclaveEquipmentSlotAssignment& Assignment)
	{
		return Assignment.SlotTag.MatchesTagExact(SlotTag);
	});
	if (Existing && Existing->Item == Item && Existing->bFromItemSource == bFromItemSource)
	{
		RefreshEquippedEffects();
		return true;
	}
	if (Existing)
	{
		if (!Existing->bFromItemSource && IsValid(Existing->Item))
		{
			Existing->Item->OnDestroyed.RemoveDynamic(this, &UEnclaveEquipmentComponent::HandleAssignedItemDestroyed);
		}
		Existing->Item = Item;
		Existing->bFromItemSource = bFromItemSource;
	}
	else
	{
		FEnclaveEquipmentSlotAssignment& NewAssignment = SlotAssignments.AddDefaulted_GetRef();
		NewAssignment.SlotTag = SlotTag;
		NewAssignment.Item = Item;
		NewAssignment.bFromItemSource = bFromItemSource;
	}

	if (!bFromItemSource)
	{
		Item->OnDestroyed.AddUniqueDynamic(this, &UEnclaveEquipmentComponent::HandleAssignedItemDestroyed);
	}

	// Equipping is assignment-owned, not wield-owned. Reconcile passive effects
	// even for armor and even if this slot isn't currently selected.
	RefreshEquippedEffects();

	// Replacing the active slot transitions to its new item through the normal
	// GAS/readiness/presentation wield path.
	if (SelectedSlotTag.MatchesTagExact(SlotTag))
	{
		return SelectSlotInternal(SlotTag, /*bAllowStowed=*/true);
	}
	return true;
}

bool UEnclaveEquipmentComponent::ClearSlot(FGameplayTag SlotTag)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}
	if (const FEnclaveEquipmentSlotAssignment* Existing = FindSlotAssignment(SlotTag))
	{
		if (!Existing->bFromItemSource && IsValid(Existing->Item))
		{
			Existing->Item->OnDestroyed.RemoveDynamic(this, &UEnclaveEquipmentComponent::HandleAssignedItemDestroyed);
		}
	}
	const int32 Removed = SlotAssignments.RemoveAll([SlotTag](const FEnclaveEquipmentSlotAssignment& Assignment)
	{
		return Assignment.SlotTag.MatchesTagExact(SlotTag);
	});
	if (Removed > 0)
	{
		RefreshEquippedEffects();
		ReconcileSelectionAfterSlotChange();
	}
	return Removed > 0;
}

bool UEnclaveEquipmentComponent::SelectSlot(FGameplayTag SlotTag)
{
	return SelectSlotInternal(SlotTag, /*bAllowStowed=*/false);
}

bool UEnclaveEquipmentComponent::SelectSlotInternal(FGameplayTag SlotTag, bool bAllowStowed)
{
	// H/ToggleStowed is the only entry into Stowed. Ordinary selection and
	// inventory UI cannot bypass its one-value return-slot bookkeeping.
	if (!bAllowStowed && SlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
	{
		return false;
	}
	const FEnclaveEquipmentSlotDefinition* Definition = FindSlotDefinition(SlotTag);
	AEnclaveItem* Item = GetItemInSlot(SlotTag);
	if (!Definition || !(Definition->bCanWield || Definition->bInWieldCycle) || !IsValid(Item))
	{
		return false;
	}
	if (!WieldItem(Item))
	{
		return false;
	}
	SelectedSlotTag = SlotTag;
	return true;
}

void UEnclaveEquipmentComponent::OnRep_SlotAssignments()
{
	// Assignment references are available to UI/presentation observers. Wielded
	// presentation remains driven solely by OnRep_CurrentWieldedItem.
}

bool UEnclaveEquipmentComponent::WieldItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsAvailableWieldable(Item))
	{
		return false;
	}

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
		IsValid(ASC) && ASC->HasMatchingGameplayTag(EnclaveGameplayTags::State_Lifecycle_Dead))
	{
		return false;
	}

	if (CurrentWieldedItem == Item)
	{
		return true;
	}

	AEnclaveItem* PreviousItem = CurrentWieldedItem;
	const bool bLeavingStowed = SelectedSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed) &&
		Item != GetItemInSlot(EnclaveGameplayTags::Equipment_Slot_Stowed);

	// A wield transition interrupts whatever the previously wielded item was
	// doing before its specs are revoked. The cancellation is generic by GAS
	// ability identity; Equipment knows no concrete Fire/Reload/etc. classes.
	CancelActiveItemAbilities();

	// Authority owns the gameplay transition. Presentation is notified only
	// after the authoritative equipment state has been committed and cannot
	// reject or roll it back.
	RemoveGrantedAbilities();
	RemoveWieldedEffects();
	CurrentWieldedItem = Item;
	for (const FEnclaveEquipmentSlotAssignment& Assignment : SlotAssignments)
	{
		if (Assignment.Item == Item)
		{
			SelectedSlotTag = Assignment.SlotTag;
			break;
		}
	}

	if (bLeavingStowed)
	{
		ReturnSlotTag = FGameplayTag();
	}

	// Actor-level visibility is replicated gameplay state. Equipment owns this
	// transition; AvatarPresentation only controls local mesh visibility.
	UpdateWieldedItemActorVisibility(PreviousItem, CurrentWieldedItem);

	BeginCurrentWieldedItemEquipWindow(CurrentWieldedItem);
	GrantAbilitiesForItem(CurrentWieldedItem);
	ApplyWieldedEffects(CurrentWieldedItem);

	UpdateWieldedItemPresentation(PreviousItem, CurrentWieldedItem);
	LocallyWieldedItem = CurrentWieldedItem;

	return true;
}

void UEnclaveEquipmentComponent::ClearCurrentWieldedItem()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	AEnclaveItem* PreviousItem = CurrentWieldedItem;

	// Preserve the original cleanup ordering: cancel active item work while
	// ability specs still exist, then revoke grants and equip readiness.
	CancelActiveItemAbilities();
	RemoveGrantedAbilities();
	RemoveWieldedEffects();
	ClearCurrentWieldedItemEquipWindow();

	CurrentWieldedItem = nullptr;
	SelectedSlotTag = FGameplayTag();
	ReturnSlotTag = FGameplayTag();
	UpdateWieldedItemActorVisibility(PreviousItem, nullptr);
	UpdateWieldedItemPresentation(PreviousItem, nullptr);
	LocallyWieldedItem.Reset();
	// Do not reset ManagedAbilitySystem here: equipped armor may still have
	// passive GE handles on this same (PlayerState-owned) ASC. They outlive wielding.
}

const FGameplayTagContainer& UEnclaveEquipmentComponent::GetInputFilters() const
{
	return InputFilters;
}

EEnclaveInputNetworkPolicy UEnclaveEquipmentComponent::GetInputNetworkPolicy(const FEnclaveInputEvent& InputEvent) const
{
	const bool bIsWieldInput =
		(NextWieldableInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(NextWieldableInputTag)) ||
		(PreviousWieldableInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PreviousWieldableInputTag)) ||
		(ToggleStowedInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ToggleStowedInputTag));

	// Wielding is a discrete command. Only relay the Started edge; forwarding
	// Triggered/Ongoing/Completed would create redundant reliable RPCs and a held
	// key could otherwise emit one every frame depending on the InputAction setup.
	if (bIsWieldInput && InputEvent.TriggerEvent == ETriggerEvent::Started)
	{
		return EEnclaveInputNetworkPolicy::ServerAuthoritative;
	}

	// Item actions remain local here because their Gameplay Abilities own their
	// own prediction/replication/authority path. Non-Started wield events are
	// also local no-ops; HandleInput already ignores them.
	return EEnclaveInputNetworkPolicy::Local;
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

	if (ToggleStowedInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(ToggleStowedInputTag))
	{
		return HandleToggleStowedInput(InputEvent);
	}

	if (InputEvent.InputTag.MatchesTag(EnclaveGameplayTags::Input_Item_Action))
	{
		return HandleItemInput(InputEvent);
	}

	return false;
}

bool UEnclaveEquipmentComponent::TryWieldNext()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	return TryWieldInDirection(1);
}

bool UEnclaveEquipmentComponent::TryWieldPrevious()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	return TryWieldInDirection(-1);
}

bool UEnclaveEquipmentComponent::CanCycleWieldables() const
{
	TArray<FGameplayTag> CycleSlots;
	BuildCycleSlotTags(CycleSlots);
	const FEnclaveEquipmentSlotDefinition* SelectedDefinition = FindSlotDefinition(SelectedSlotTag);
	const bool bSelectedIsInCycle = SelectedDefinition &&
		(SelectedSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Default)
			? DefaultReservedDefinition.bInWieldCycle : SelectedDefinition->bInWieldCycle);
	// Stowed or any other non-cycle selection can enter the one available slot.
	return bSelectedIsInCycle ? CycleSlots.Num() > 1 : !CycleSlots.IsEmpty();
}

bool UEnclaveEquipmentComponent::HandleItemInput(const FEnclaveInputEvent& InputEvent)
{
	const FGameplayTag& InputTag = InputEvent.InputTag;
	const bool bRelease = InputEvent.TriggerEvent == ETriggerEvent::Completed || InputEvent.TriggerEvent == ETriggerEvent::Canceled;

	// Never re-resolve a release against a newly wielded item. A revoked spec
	// is tolerated by the ASC (e.g. the weapon was switched mid-hold).
	if (bRelease)
	{
		FGameplayAbilitySpecHandle HeldHandle;
		if (!HeldItemInputSpecs.RemoveAndCopyValue(InputTag, HeldHandle))
		{
			return false;
		}
		if (UEnclaveAbilitySystemComponent* ASC = Cast<UEnclaveAbilitySystemComponent>(GetOwnerAbilitySystemComponent()))
		{
			ASC->ReleaseAbilityInput(HeldHandle);
		}
		return true;
	}

	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	// A new Started while this input is tracked means a configured release
	// event was missed. Release the old spec before handling the new press.
	FGameplayAbilitySpecHandle StaleHandle;
	if (HeldItemInputSpecs.RemoveAndCopyValue(InputTag, StaleHandle))
	{
		if (UEnclaveAbilitySystemComponent* StaleASC = Cast<UEnclaveAbilitySystemComponent>(GetOwnerAbilitySystemComponent()))
		{
			StaleASC->ReleaseAbilityInput(StaleHandle);
		}
	}

	const UEnclaveItemDefinition* Definition = IsValid(CurrentWieldedItem) ? CurrentWieldedItem->GetItemDefinition() : nullptr;
	if (IsValid(Definition))
	{
		if (const FEnclaveItemActionBinding* Binding = Definition->FindActionBinding(InputTag))
		{
			switch (Binding->Mode)
			{
			case EEnclaveItemActionBindingMode::Suppressed:
				return true;
			case EEnclaveItemActionBindingMode::CharacterFallback:
				// Character capability routing belongs to Phase 4.
				return false;
			case EEnclaveItemActionBindingMode::ItemAbility:
				break;
			default:
				return false;
			}
		}
	}

	const FGameplayAbilitySpecHandle SpecHandle = FindWieldedItemAbilityForInput(InputTag);
	UEnclaveAbilitySystemComponent* ASC = Cast<UEnclaveAbilitySystemComponent>(GetOwnerAbilitySystemComponent());
	if (!SpecHandle.IsValid() || !IsValid(ASC))
	{
		return false;
	}

	// Only a binding with release events needs an equipment-level hold.
	// Started-only abilities end through their own gameplay lifecycle.
	if (InputEvent.bRoutesReleaseEvents)
	{
		HeldItemInputSpecs.Add(InputTag, SpecHandle);
	}
	return InputEvent.bRoutesReleaseEvents
		? ASC->PressAbilityInput(SpecHandle)
		: ASC->ActivateAbilityFromInput(SpecHandle);
}

void UEnclaveEquipmentComponent::ReleaseHeldItemInputs()
{
	UEnclaveAbilitySystemComponent* ASC = Cast<UEnclaveAbilitySystemComponent>(GetOwnerAbilitySystemComponent());
	if (IsValid(ASC))
	{
		for (const TPair<FGameplayTag, FGameplayAbilitySpecHandle>& Entry : HeldItemInputSpecs)
		{
			ASC->ReleaseAbilityInput(Entry.Value);
		}
	}
	HeldItemInputSpecs.Reset();
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

	if (ToggleStowedInputTag.IsValid())
	{
		InputFilters.AddTag(ToggleStowedInputTag);
	}

	InputFilters.AddTag(EnclaveGameplayTags::Input_Item_Action);
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

bool UEnclaveEquipmentComponent::HandleToggleStowedInput(const FEnclaveInputEvent& InputEvent)
{
	return InputEvent.TriggerEvent == ETriggerEvent::Started && ToggleStowed();
}

bool UEnclaveEquipmentComponent::ToggleStowed()
{
	AActor* OwnerActor = GetOwner();
	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	if (SelectedSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
	{
		return RestoreStowedSelection();
	}

	if (!IsValid(CurrentWieldedItem) || !SelectedSlotTag.IsValid() ||
		!IsValid(GetItemInSlot(EnclaveGameplayTags::Equipment_Slot_Stowed)))
	{
		return false;
	}

	const FGameplayTag PreviousSlot = SelectedSlotTag;
	if (!SelectSlotInternal(EnclaveGameplayTags::Equipment_Slot_Stowed, /*bAllowStowed=*/true))
	{
		return false;
	}
	ReturnSlotTag = PreviousSlot;
	return true;
}

bool UEnclaveEquipmentComponent::RestoreStowedSelection()
{
	AActor* OwnerActor = GetOwner();
	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority() ||
		!SelectedSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed))
	{
		return false;
	}

	// A reassigned slot restores its current occupant. An emptied or destroyed
	// assignment returns to Default instead. Never select Stowed recursively.
	if (ReturnSlotTag.IsValid() &&
		!ReturnSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed) &&
		IsValid(GetItemInSlot(ReturnSlotTag)) && SelectSlot(ReturnSlotTag))
	{
		ReturnSlotTag = FGameplayTag();
		return true;
	}
	if (!DefaultSelectionSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed) &&
		SelectSlot(DefaultSelectionSlotTag))
	{
		ReturnSlotTag = FGameplayTag();
		return true;
	}
	return false; // Nothing valid to restore; keep Stowed selected.
}

bool UEnclaveEquipmentComponent::TryWieldInDirection(int32 Direction)
{
	if ((Direction != 1 && Direction != -1) || !CanCycleWieldables())
	{
		return false;
	}

	TArray<FGameplayTag> CycleSlots;
	BuildCycleSlotTags(CycleSlots);
	if (CycleSlots.IsEmpty())
	{
		return false;
	}

	// While Stowed, scroll relative to the saved slot rather than resetting
	// cycle position. Missing return slots retain the normal directional start.
	const FGameplayTag CycleAnchor = SelectedSlotTag.MatchesTagExact(EnclaveGameplayTags::Equipment_Slot_Stowed)
		? ReturnSlotTag : SelectedSlotTag;
	int32 Index = CycleSlots.IndexOfByKey(CycleAnchor);
	if (Index == INDEX_NONE)
	{
		Index = Direction > 0 ? -1 : 0;
	}
	for (int32 Step = 0; Step < CycleSlots.Num(); ++Step)
	{
		Index = (Index + Direction + CycleSlots.Num()) % CycleSlots.Num();
		if (SelectSlot(CycleSlots[Index]))
		{
			return true;
		}
	}
	return false;
}

bool UEnclaveEquipmentComponent::IsAvailableWieldable(const AEnclaveItem* Item) const
{
	if (!IsValid(Item) || !Item->HasItemCapability(EnclaveGameplayTags::Item_Capability_Wieldable))
	{
		return false;
	}
	for (const FEnclaveEquipmentSlotAssignment& Assignment : SlotAssignments)
	{
		const FEnclaveEquipmentSlotDefinition* Definition = FindSlotDefinition(Assignment.SlotTag);
		if (Definition && (Definition->bCanWield || Definition->bInWieldCycle) && Assignment.Item == Item && IsAssignmentValid(Assignment))
		{
			return true;
		}
	}
	return false;
}

void UEnclaveEquipmentComponent::UnbindItemSource()
{
	if (ItemSource)
	{
		ItemSource->OnItemSourceChanged().RemoveAll(this);
	}
	ItemSource.SetInterface(nullptr);
	ItemSource.SetObject(nullptr);
}

void UEnclaveEquipmentComponent::HandleItemSourceChanged()
{
	AActor* OwnerActor = GetOwner();
	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	for (int32 Index = SlotAssignments.Num() - 1; Index >= 0; --Index)
	{
		const FEnclaveEquipmentSlotAssignment& Assignment = SlotAssignments[Index];
		if (Assignment.bFromItemSource && !IsAssignmentValid(Assignment))
		{
			SlotAssignments.RemoveAt(Index);
		}
	}
	RefreshEquippedEffects();
	ReconcileSelectionAfterSlotChange();
}

void UEnclaveEquipmentComponent::HandleAssignedItemDestroyed(AActor* DestroyedActor)
{
	AActor* OwnerActor = GetOwner();
	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	const int32 Removed = SlotAssignments.RemoveAll([DestroyedActor](const FEnclaveEquipmentSlotAssignment& Assignment)
	{
		return !Assignment.bFromItemSource && Assignment.Item.Get() == DestroyedActor;
	});
	if (Removed > 0)
	{
		RefreshEquippedEffects();
		ReconcileSelectionAfterSlotChange();
	}
}

void UEnclaveEquipmentComponent::ReconcileSelectionAfterSlotChange()
{
	AActor* OwnerActor = GetOwner();
	if (bEquipmentTearingDown || !IsValid(OwnerActor) || !OwnerActor->HasAuthority() ||
		OwnerActor->IsActorBeingDestroyed())
	{
		return;
	}

	// Initial loadout selection is explicit, after the owner has assigned
	// its starting slots. A source change must not select prematurely.
	if (!CurrentWieldedItem && !SelectedSlotTag.IsValid())
	{
		return;
	}
	if (IsAvailableWieldable(CurrentWieldedItem))
	{
		return;
	}

	// Invalidating the active slot must revoke its abilities. Never draw a
	// fallback on a dead avatar or while Equipment is tearing down.
	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
		IsValid(ASC) && ASC->HasMatchingGameplayTag(EnclaveGameplayTags::State_Lifecycle_Dead))
	{
		ClearCurrentWieldedItem();
		return;
	}

	if (!SelectSlot(DefaultSelectionSlotTag))
	{
		ClearCurrentWieldedItem();
	}
}

IEnclaveAvatarPresentationInterface* UEnclaveEquipmentComponent::GetAvatarPresentation() const
{
	return IEnclaveAvatarPresentationInterface::FindAvatarPresentation(GetOwner());
}

void UEnclaveEquipmentComponent::BeginCurrentWieldedItemEquipWindow(AEnclaveItem* Item)
{
	ClearCurrentWieldedItemEquipWindow();

	if (!IsValid(Item))
	{
		return;
	}

	const float EquipDuration = FMath::Max(0.0f, Item->GetEquipDuration());

	if (EquipDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: cannot enforce %.3fs equip window for %s because the owner has no AbilitySystemComponent; skipping the equip window."),
			*GetNameSafe(GetOwner()), EquipDuration, *GetNameSafe(Item));
		return;
	}

	ManagedAbilitySystem = ASC;

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(Item);

	FGameplayEffectSpecHandle EffectSpec = ASC->MakeOutgoingSpec(UEnclaveGE_ItemEquipping::StaticClass(), /*Level=*/ 1.0f, EffectContext);

	if (!EffectSpec.IsValid())
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: failed to create the equip Gameplay Effect for %s; skipping the equip window."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Item));
		return;
	}

	// The item definition owns the authored duration. The native GE owns the
	// authoritative clock, State.Item.Equipping, and item-action blocking for
	// exactly that window. Equipment keeps only the handle needed for early removal.
	EffectSpec.Data->SetSetByCallerMagnitude(EnclaveGameplayTags::Data_Item_EquipDuration, EquipDuration);

	const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	if (!AppliedHandle.IsValid() || !AppliedHandle.WasSuccessfullyApplied())
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: failed to apply the equip Gameplay Effect for %s; skipping the equip window."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Item));
		return;
	}

	CurrentWieldedItemEquipEffectHandle = AppliedHandle;
}

bool UEnclaveEquipmentComponent::IsCurrentWieldedItemReadyForActions() const
{
	if (!IsValid(CurrentWieldedItem))
	{
		return false;
	}

	const UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	return IsValid(ASC) && !ASC->HasMatchingGameplayTag(EnclaveGameplayTags::State_Item_Equipping);
}

void UEnclaveEquipmentComponent::ClearCurrentWieldedItemEquipWindow()
{
	if (!CurrentWieldedItemEquipEffectHandle.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle HandleToRemove = CurrentWieldedItemEquipEffectHandle;
	CurrentWieldedItemEquipEffectHandle.Invalidate();

	if (UAbilitySystemComponent* ASC = ManagedAbilitySystem.Get())
	{
		ASC->RemoveActiveGameplayEffect(HandleToRemove);
	}
}

void UEnclaveEquipmentComponent::UpdateWieldedItemActorVisibility(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem)
{
	// This helper is called only from authority-owned wield transitions. The
	// replicated actor flag represents gameplay/world visibility: carried but
	// unwielded items are hidden, while the currently wielded item is visible.
	if (IsValid(PreviousItem) && PreviousItem != NewItem)
	{
		PreviousItem->SetActorHiddenInGame(true);
	}

	if (IsValid(NewItem))
	{
		NewItem->SetActorHiddenInGame(false);
	}
}

void UEnclaveEquipmentComponent::UpdateWieldedItemPresentation(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem)
{
	IEnclaveAvatarPresentationInterface* AvatarPresentation = GetAvatarPresentation();

	if (!AvatarPresentation)
	{
		return;
	}

	// AvatarPresentation owns the persistent presentation state it applied, so
	// always let it tear that state down before reconciliation. This still works
	// when PreviousItem has already been destroyed and the local weak pointer is stale.
	AvatarPresentation->ClearWieldedItemPresentation();

	if (IsValid(NewItem))
	{
		AvatarPresentation->PresentWieldedItem(NewItem);

		const float EquipDuration = FMath::Max(0.0f, NewItem->GetEquipDuration());

		// Equipment owns the semantic wield transition. A zero-duration equip is
		// immediately Ready and therefore has no Draw transition to present.
		// Positive durations explicitly report Draw so presentation can conform
		// its animation/choreography to the authoritative gameplay window.
		if (PreviousItem != NewItem && EquipDuration > KINDA_SMALL_NUMBER)
		{
			FEnclaveItemPresentationActionEvent DrawEvent;
			DrawEvent.Item = NewItem;
			DrawEvent.ItemType = NewItem->GetItemType();
			DrawEvent.Action = EnclaveGameplayTags::Presentation_Item_Action_Draw;
			DrawEvent.Lifecycle = EEnclaveItemPresentationActionLifecycle::OneShot;
			DrawEvent.Duration = EquipDuration;
			AvatarPresentation->HandleItemPresentationAction(DrawEvent);
		}
	}
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

void UEnclaveEquipmentComponent::CancelActiveItemAbilities()
{
	UAbilitySystemComponent* ASC = ManagedAbilitySystem.Get();

	if (!IsValid(ASC))
	{
		return;
	}

	// Lifecycle cleanup is ownership-scoped, not semantic. Cancel only the
	// ability specs granted by this Equipment instance so a late teardown from
	// an old pawn cannot cancel item abilities granted to a newer pawn on the
	// same persistent ASC.
	for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
	{
		ASC->CancelAbilityHandle(Handle);
	}
}

void UEnclaveEquipmentComponent::GrantAbilitiesForItem(AEnclaveItem* Item)
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!IsValid(ASC) || !IsValid(Item))
	{
		return;
	}

	ManagedAbilitySystem = ASC;

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
	if (UAbilitySystemComponent* ASC = ManagedAbilitySystem.Get())
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
		{
			ASC->ClearAbility(Handle);
		}
	}

	GrantedAbilityHandles.Reset();
}

void UEnclaveEquipmentComponent::ApplyItemEffects(AEnclaveItem* Item,
	const TArray<TSubclassOf<UGameplayEffect>>& EffectClasses, const TCHAR* EffectCategory,
	bool bRequireIndependentInstances, TArray<FActiveGameplayEffectHandle>& OutHandles)
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (!IsValid(Item) || !IsValid(ASC) || !GetOwner() || !GetOwner()->HasAuthority() ||
		ASC->HasMatchingGameplayTag(EnclaveGameplayTags::State_Lifecycle_Dead))
	{
		return;
	}

	ManagedAbilitySystem = ASC;
	for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectClasses)
	{
		const UGameplayEffect* Effect = EffectClass ? EffectClass->GetDefaultObject<UGameplayEffect>() : nullptr;
		if (!IsValid(Effect))
		{
			continue;
		}
		if (Effect->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: %s entry %s must have Infinite duration; skipping."),
				*GetNameSafe(Item), EffectCategory, *GetNameSafe(Effect));
			continue;
		}
		// Equipped items own their effects independently. GAS stack aggregation can
		// merge multiple items into one active handle; removing one slot would then
		// strip the other slot's benefit. Require unstacked definitions for these.
		if (bRequireIndependentInstances && Effect->GetStackingType() != EGameplayEffectStackingType::None)
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: %s entry %s must use Stacking=None for per-slot effect ownership; skipping."),
				*GetNameSafe(Item), EffectCategory, *GetNameSafe(Effect));
			continue;
		}
		if (const UEnclaveGameplayEffect* EnclaveEffect = Cast<UEnclaveGameplayEffect>(Effect);
			IsValid(EnclaveEffect) && !EnclaveEffect->GetEffectScope().MatchesTagExact(EnclaveGameplayTags::Effect_Scope_Avatar))
		{
			UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: %s entry %s must be Avatar-scoped; skipping."),
				*GetNameSafe(Item), EffectCategory, *GetNameSafe(Effect));
			continue;
		}

		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(Item);
		FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);
		if (!Spec.IsValid() || !Spec.Data.IsValid())
		{
			continue;
		}
		// Effects are owned by this avatar even though the ASC is on PlayerState.
		Spec.Data->AddDynamicAssetTag(EnclaveGameplayTags::Effect_Scope_Avatar);
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		if (Handle.IsValid() && Handle.WasSuccessfullyApplied())
		{
			OutHandles.Add(Handle);
		}
	}
}

void UEnclaveEquipmentComponent::RemoveEffectHandles(TArray<FActiveGameplayEffectHandle>& Handles)
{
	if (UAbilitySystemComponent* ASC = ManagedAbilitySystem.Get())
	{
		for (const FActiveGameplayEffectHandle& Handle : Handles)
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}
	Handles.Reset();
}

void UEnclaveEquipmentComponent::ApplyWieldedEffects(AEnclaveItem* Item)
{
	if (IsValid(Item))
	{
		ApplyItemEffects(Item, Item->GetEffectsWhileWielded(), TEXT("EffectsWhileWielded"), /*bRequireIndependentInstances=*/false, WieldedEffectHandles);
	}
}

void UEnclaveEquipmentComponent::RemoveWieldedEffects()
{
	RemoveEffectHandles(WieldedEffectHandles);
}

void UEnclaveEquipmentComponent::RemoveAllEquippedEffects()
{
	for (TPair<FGameplayTag, FEnclaveEquippedItemEffectState>& Entry : EquippedEffectStates)
	{
		RemoveEffectHandles(Entry.Value.EffectHandles);
	}
	EquippedEffectStates.Reset();
}

void UEnclaveEquipmentComponent::RefreshEquippedEffects()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();
	if (ManagedAbilitySystem.IsValid() && ManagedAbilitySystem.Get() != ASC)
	{
		// In the unusual case of ASC replacement, never let old slot handles
		// survive on the previous ASC. The new one will receive fresh grants.
		RemoveAllEquippedEffects();
	}
	if (!IsValid(ASC) || bEquipmentTearingDown ||
		ASC->HasMatchingGameplayTag(EnclaveGameplayTags::State_Lifecycle_Dead))
	{
		RemoveAllEquippedEffects();
		return;
	}

	// Remove effects before applying replacements. A slot assignment that has
	// changed occupant, source membership, or validity must never retain the
	// previous occupant's passive stats.
	for (auto It = EquippedEffectStates.CreateIterator(); It; ++It)
	{
		const FEnclaveEquipmentSlotAssignment* Assignment = FindSlotAssignment(It.Key());
		if (!Assignment || !IsAssignmentValid(*Assignment) || Assignment->Item.Get() != It.Value().Item.Get())
		{
			RemoveEffectHandles(It.Value().EffectHandles);
			It.RemoveCurrent();
		}
	}

	for (const FEnclaveEquipmentSlotAssignment& Assignment : SlotAssignments)
	{
		if (!IsAssignmentValid(Assignment) || EquippedEffectStates.Contains(Assignment.SlotTag))
		{
			continue;
		}
		AEnclaveItem* Item = Assignment.Item.Get();
		FEnclaveEquippedItemEffectState& State = EquippedEffectStates.Add(Assignment.SlotTag);
		State.Item = Item;
		ApplyItemEffects(Item, Item->GetEffectsWhileEquipped(), TEXT("EffectsWhileEquipped"), /*bRequireIndependentInstances=*/true, State.EffectHandles);
	}
}

void UEnclaveEquipmentComponent::OnRep_CurrentWieldedItem()
{
	AEnclaveItem* PreviousItem = LocallyWieldedItem.Get();

	UpdateWieldedItemPresentation(PreviousItem, CurrentWieldedItem);
	LocallyWieldedItem = CurrentWieldedItem;
}
