// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Abilities/Effects/EnclaveGE_ItemEquipping.h"
#include "Debug/EnclaveDebug.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayTags/EnclaveInputTags.h"
#include "GameplayTags/EnclaveItemTags.h"
#include "GameplayTags/EnclavePresentationTags.h"
#include "GameplayTags/EnclaveAbilityTags.h"

#include "AbilitySystemComponent.h"
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
	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentWieldedItemReadiness);
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
	BeginCurrentWieldedItemReadinessTransition(CurrentWieldedItem);
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
	ClearCurrentWieldedItemReadinessTransition();

	CurrentWieldedItem = nullptr;
	CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Unequipped;

	UpdateWieldedItemPresentation(PreviousItem, nullptr);
	LocallyWieldedItem.Reset();
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

	if (InputEvent.InputTag.MatchesTag(EnclaveGameplayTags::Input_Item_Action))
	{
		return HandleItemInput(InputEvent);
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

bool UEnclaveEquipmentComponent::HandleItemInput(const FEnclaveInputEvent& InputEvent)
{
	// For now, item abilities are press-to-activate. A later GAS input pass can
	// forward pressed/released state generically for held, charged, automatic,
	// or release-driven abilities without restoring action-specific handlers.
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	const FGameplayAbilitySpecHandle SpecHandle = FindWieldedItemAbilityForInput(InputEvent.InputTag);
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!SpecHandle.IsValid() || !IsValid(ASC))
	{
		return false;
	}

	// A matching spec is the wielded item's declaration that it supports this
	// input. Whether the action is currently legal belongs to the ability's
	// CanActivateAbility/CommitAbility path, not Equipment.
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

void UEnclaveEquipmentComponent::BeginCurrentWieldedItemReadinessTransition(AEnclaveItem* Item)
{
	ClearCurrentWieldedItemReadinessTransition();

	if (!IsValid(Item))
	{
		CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Unequipped;
		return;
	}

	const float EquipDuration = FMath::Max(0.0f, Item->GetEquipDuration());

	if (EquipDuration <= KINDA_SMALL_NUMBER)
	{
		CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: cannot enforce %.3fs equip readiness for %s because the owner has no AbilitySystemComponent; failing open as Ready."),
			*GetNameSafe(GetOwner()), EquipDuration, *GetNameSafe(Item));
		CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(Item);

	FGameplayEffectSpecHandle EffectSpec = ASC->MakeOutgoingSpec(UEnclaveGE_ItemEquipping::StaticClass(), /*Level=*/ 1.0f, EffectContext);

	if (!EffectSpec.IsValid())
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: failed to create the equip-readiness Gameplay Effect for %s; failing open as Ready."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Item));
		CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	// The weapon/item definition owns the authored duration. The native GE owns
	// the clock, and its granted tag owns ability blocking for exactly that same
	// window. There is deliberately no parallel Equipment timer.
	EffectSpec.Data->SetSetByCallerMagnitude(EnclaveGameplayTags::Data_Item_EquipDuration, EquipDuration);

	const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*EffectSpec.Data.Get());

	if (!AppliedHandle.IsValid() || !AppliedHandle.WasSuccessfullyApplied())
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: failed to apply the equip-readiness Gameplay Effect for %s; failing open as Ready."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Item));
		CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	CurrentWieldedItemEquipEffectHandle = AppliedHandle;
	CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Equipping;

	FActiveGameplayEffectEvents* EffectEvents = ASC->GetActiveEffectEventSet(AppliedHandle);

	if (!EffectEvents)
	{
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: equip-readiness Gameplay Effect for %s has no active-effect event set; removing it and failing open as Ready."),
			*GetNameSafe(GetOwner()), *GetNameSafe(Item));
		ASC->RemoveActiveGameplayEffect(AppliedHandle);
		CurrentWieldedItemEquipEffectHandle.Invalidate();
		CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	EffectEvents->OnEffectRemoved.AddUObject(this, &UEnclaveEquipmentComponent::HandleCurrentWieldedItemEquipEffectRemoved, AppliedHandle, TWeakObjectPtr<AEnclaveItem>(Item));
}

void UEnclaveEquipmentComponent::HandleCurrentWieldedItemEquipEffectRemoved(const FGameplayEffectRemovalInfo& /*RemovalInfo*/, FActiveGameplayEffectHandle RemovedHandle, TWeakObjectPtr<AEnclaveItem> TransitionItem)
{
	if (CurrentWieldedItemEquipEffectHandle == RemovedHandle)
	{
		CurrentWieldedItemEquipEffectHandle.Invalidate();
	}

	AEnclaveItem* CompletedItem = TransitionItem.Get();

	// An old effect can be explicitly removed while switching to a new item.
	// Only the effect belonging to the still-current wield relationship is
	// allowed to advance readiness.
	if (!IsValid(CompletedItem) || CurrentWieldedItem != CompletedItem)
	{
		return;
	}

	CurrentWieldedItemReadiness = EEnclaveEquipmentReadiness::Ready;
}

void UEnclaveEquipmentComponent::ClearCurrentWieldedItemReadinessTransition()
{
	if (!CurrentWieldedItemEquipEffectHandle.IsValid())
	{
		return;
	}

	const FActiveGameplayEffectHandle HandleToRemove = CurrentWieldedItemEquipEffectHandle;
	CurrentWieldedItemEquipEffectHandle.Invalidate();

	if (UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent())
	{
		ASC->RemoveActiveGameplayEffect(HandleToRemove);
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

void UEnclaveEquipmentComponent::CancelActiveItemAbilities()
{
	UAbilitySystemComponent* ASC = GetOwnerAbilitySystemComponent();

	if (!IsValid(ASC))
	{
		return;
	}

	FGameplayTagContainer ItemActionTags;
	ItemActionTags.AddTag(EnclaveGameplayTags::Ability_Type_Action_Item);
	ASC->CancelAbilities(&ItemActionTags, /*WithoutTags=*/ nullptr, /*Ignore=*/ nullptr);
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
