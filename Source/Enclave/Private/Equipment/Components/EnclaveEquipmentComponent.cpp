// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayTags/EnclaveInputTags.h"
#include "GameplayTags/EnclavePresentationTags.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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
	ClearEquipmentReadinessTransition();
	UnbindItemSource();

	Super::UninitializeComponent();
}

void UEnclaveEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentWieldedItem);
	DOREPLIFETIME(UEnclaveEquipmentComponent, EquipmentReadiness);
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
	BeginEquipmentReadinessTransition(CurrentWieldedItem);

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
		ClearEquipmentReadinessTransition();
		EquipmentReadiness = EEnclaveEquipmentReadiness::Unequipped;
		UpdateWieldedItemPresentation(PreviousItem, nullptr);
		LocallyWieldedItem.Reset();
	}
}

IEnclaveAvatarPresentationInterface* UEnclaveEquipmentComponent::GetAvatarPresentation() const
{
	return IEnclaveAvatarPresentationInterface::FindAvatarPresentation(GetOwner());
}

void UEnclaveEquipmentComponent::BeginEquipmentReadinessTransition(AEnclaveItem* Item)
{
	ClearEquipmentReadinessTransition();

	if (!IsValid(Item))
	{
		EquipmentReadiness = EEnclaveEquipmentReadiness::Unequipped;
		return;
	}

	const float EquipDuration = FMath::Max(0.0f, Item->GetEquipDuration());

	if (EquipDuration <= KINDA_SMALL_NUMBER)
	{
		EquipmentReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	EquipmentReadiness = EEnclaveEquipmentReadiness::Equipping;

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		// With no world there is no authoritative clock to finish the delay.
		// Failing open during teardown/initialization is safer than leaving the
		// component permanently Equipping.
		EquipmentReadiness = EEnclaveEquipmentReadiness::Ready;
		return;
	}

	World->GetTimerManager().SetTimer(EquipmentReadinessTimerHandle, this, &UEnclaveEquipmentComponent::CompleteEquipmentReadinessTransition, EquipDuration, false);
}

void UEnclaveEquipmentComponent::CompleteEquipmentReadinessTransition()
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	EquipmentReadiness = IsValid(CurrentWieldedItem)
		? EEnclaveEquipmentReadiness::Ready
		: EEnclaveEquipmentReadiness::Unequipped;
}

void UEnclaveEquipmentComponent::ClearEquipmentReadinessTransition()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EquipmentReadinessTimerHandle);
	}
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
			DrawEvent.Item = NewItem;
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
