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

#include "AbilitySystemComponent.h"
#include "Abilities/EnclaveAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"

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
	ReleaseHeldItemInputs();
	AActor* OwnerActor = GetOwner();

	// The ASC is PlayerState-owned and can outlive this Pawn. Revoke every
	// Pawn/item-scoped gameplay contribution before this component goes away so
	// a non-death teardown (travel, admin destruction, etc.) cannot leak specs or
	// an active equip effect into the next avatar.
	if (IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		ClearCurrentWieldedItem();
	}

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

	// A wield transition interrupts whatever the previously wielded item was
	// doing before its specs are revoked. The cancellation is generic by GAS
	// ability identity; Equipment knows no concrete Fire/Reload/etc. classes.
	CancelActiveItemAbilities();

	// Authority owns the gameplay transition. Presentation is notified only
	// after the authoritative equipment state has been committed and cannot
	// reject or roll it back.
	RemoveGrantedAbilities();
	CurrentWieldedItem = Item;

	// Actor-level visibility is replicated gameplay state. Equipment owns this
	// transition; AvatarPresentation only controls local mesh visibility.
	UpdateWieldedItemActorVisibility(PreviousItem, CurrentWieldedItem);

	BeginCurrentWieldedItemEquipWindow(CurrentWieldedItem);
	GrantAbilitiesForItem(CurrentWieldedItem);

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

	// Order matters: cancel active item work while its specs/source object still
	// exist, then revoke the specs and any authoritative equip-window effect.
	CancelActiveItemAbilities();
	RemoveGrantedAbilities();
	ClearCurrentWieldedItemEquipWindow();

	CurrentWieldedItem = nullptr;

	// The released item remains carried in the current slice, so gameplay hides
	// the actor again. Presentation independently tears down its local mesh state.
	UpdateWieldedItemActorVisibility(PreviousItem, nullptr);
	UpdateWieldedItemPresentation(PreviousItem, nullptr);
	LocallyWieldedItem.Reset();

	// All GAS state owned by this Equipment instance has now been revoked.
	ManagedAbilitySystem.Reset();
}

const FGameplayTagContainer& UEnclaveEquipmentComponent::GetInputFilters() const
{
	return InputFilters;
}

EEnclaveInputNetworkPolicy UEnclaveEquipmentComponent::GetInputNetworkPolicy(const FEnclaveInputEvent& InputEvent) const
{
	const bool bIsWieldInput =
		(NextWieldableInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(NextWieldableInputTag)) ||
		(PreviousWieldableInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PreviousWieldableInputTag));

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
	return IsValid(CurrentWieldedItem) ? WieldableCache.Num() > 1 : WieldableCache.Num() > 0;
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
		if (IsValid(Item) && Item->HasItemCapability(EnclaveGameplayTags::Item_Capability_Wieldable))
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

void UEnclaveEquipmentComponent::OnRep_CurrentWieldedItem()
{
	AEnclaveItem* PreviousItem = LocallyWieldedItem.Get();

	UpdateWieldedItemPresentation(PreviousItem, CurrentWieldedItem);
	LocallyWieldedItem = CurrentWieldedItem;
}
