// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "Inventory/Interfaces/EnclaveItemSourceInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "EnclaveEquipmentComponent.generated.h"

class AEnclaveItem;
class UAbilitySystemComponent;
class IEnclaveAvatarPresentationInterface;
class UGameplayEffect;

/** Describes an assignable position; bCanWield and bInWieldCycle are separate policies. */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveEquipmentSlotDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FGameplayTag SlotTag;

	/** Whether the assigned item can be actively wielded. Non-wieldable slots
	 * may still hold items and grant EffectsWhileEquipped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	bool bCanWield = false;

	/** Whether this wieldable slot participates in Next/Previous. Requires bCanWield. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (EditCondition = "bCanWield"))
	bool bInWieldCycle = false;

	/** Additional item capabilities required for an assignment to this slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FGameplayTagContainer RequiredCapabilities;
};

/** A server-authoritative, replicated reference. Item lifetime stays with its owner. */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveEquipmentSlotAssignment
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	FGameplayTag SlotTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	TObjectPtr<AEnclaveItem> Item = nullptr;

	/** True if membership must remain valid in the single assigned item source. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	bool bFromItemSource = false;
};

// Non-reflected, non-replicated runtime state. UObject references here are weak;
// all GC-visible and replicated references remain UPROPERTYs on the component.
// These types group ownership only; they do not introduce a second lifecycle.

/** Tracks one validated slot occupant and precisely the passive effects granted for it. */
struct FEnclaveEquippedItemEffectState
{
	TWeakObjectPtr<AEnclaveItem> Item;
	TArray<FActiveGameplayEffectHandle> EffectHandles;
};

/** Assignment domain: reserved slot policies and passive effects owned by slot occupants. */
struct FEnclaveEquipmentAssignmentRuntimeState
{
	FEnclaveEquipmentSlotDefinition DefaultReservedDefinition;
	FEnclaveEquipmentSlotDefinition StowedReservedDefinition;

	// Authority-only effects per assignment, independent of the active wield selection.
	// Empty-effect entries are retained to avoid reapplying on every reconciliation.
	TMap<FGameplayTag, FEnclaveEquippedItemEffectState> EquippedEffectStates;
};

/** Wield domain: transient active-item gameplay; never replicated separately. */
struct FEnclaveEquipmentWieldRuntimeState
{
	// A press retains its resolved ability spec until release, even across a switch.
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> HeldItemInputSpecs;
	// Physical item-action tag -> character semantic tag, pinned until release.
	// This mapping survives weapon switches so release cannot target the new item.
	TMap<FGameplayTag, FGameplayTag> HeldCharacterInputTags;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
	TArray<FActiveGameplayEffectHandle> WieldedEffectHandles;
	FActiveGameplayEffectHandle CurrentWieldedItemEquipEffectHandle;
	FGameplayTag ReturnSlotTag;
	TWeakObjectPtr<AEnclaveItem> LocallyWieldedItem;
	FGameplayTagContainer InputFilters;
};

/**
 * Owns slot assignments, selection, wielding, GAS grants/readiness, and
 * presentation notifications. Does NOT create/destroy items or discover sources.
 * The owner explicitly sets one source; external items require server-authorized
 * assignment to a configured slot. Only assigned wield-cycle slots are cycled.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveEquipmentComponent : public UActorComponent, public IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:
	UEnclaveEquipmentComponent();

	// --- UActorComponent: shared lifetime and replication boundary ---
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UninitializeComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Assignment domain: source, slot occupancy and passive equipped effects ---
	/** Explicit, owner-provided source. Does not take ownership of the component. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Source")
	bool SetItemSource(UActorComponent* SourceComponent);

	/** Source items must still belong to this Equipment's assigned source. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slots")
	bool AssignSourceItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item);

	/** External items are owned by the Equipment actor, but never by Equipment itself. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slots")
	bool AssignExternalItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Equipment|Slots")
	bool ClearSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "Equipment|Slots")
	AEnclaveItem* GetItemInSlot(FGameplayTag SlotTag) const;

	const TArray<FEnclaveEquipmentSlotAssignment>& GetSlotAssignments() const { return SlotAssignments; }

	/** Server-only reconciliation when the owner first binds (or rebinds) its ASC.
	 * Called automatically on assignment changes; harmless if the ASC is absent. */
	void RefreshEquippedEffects();

	// --- Wield domain: active selection, input, readiness, abilities and presentation ---
	UFUNCTION(BlueprintCallable, Category = "Equipment|Slots")
	bool SelectSlot(FGameplayTag SlotTag);

	/** Authority-only teardown of the active wield relationship; does not remove assignments. */
	void ClearCurrentWieldedItem();
	AEnclaveItem* GetCurrentWieldedItem() const { return CurrentWieldedItem; }

	/** GAS is the readiness source of truth: State.Item.Equipping blocks item actions. */
	bool IsCurrentWieldedItemReadyForActions() const;

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual EEnclaveInputNetworkPolicy GetInputNetworkPolicy(const FEnclaveInputEvent& InputEvent) const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	bool TryWieldNext();
	bool TryWieldPrevious();
	bool CanCycleWieldables() const;

	/** Server-authoritative toggle; has no effect without an assigned Stowed item. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Stowed")
	bool ToggleStowed();

	/** Called by Unstow GAS ability on server; falls back to Default if necessary. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Stowed")
	bool RestoreStowedSelection();

private:
	// --- Assignment domain ---
	void ValidateSlotDefinitions() const;
	bool IsAssignmentValid(const FEnclaveEquipmentSlotAssignment& Assignment) const;
	bool AssignItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item, bool bFromItemSource);
	const FEnclaveEquipmentSlotDefinition* FindSlotDefinition(FGameplayTag SlotTag) const;
	const FEnclaveEquipmentSlotAssignment* FindSlotAssignment(FGameplayTag SlotTag) const;
	void UnbindItemSource();
	void HandleItemSourceChanged();

	/** An external owner may destroy its item independently of its slot. */
	UFUNCTION()
	void HandleAssignedItemDestroyed(AActor* DestroyedActor);

	/** Return assigned slots whose owned Gameplay Effects disappeared from the ASC. */
	TSet<FGameplayTag> ReconcileTrackedEffectHandles(UAbilitySystemComponent* AbilitySystemComponent);
	void RemoveAllEquippedEffects();

	// --- Wield domain ---
	/** Stowed can only be entered via the dedicated toggle entry path. */
	bool SelectSlotInternal(FGameplayTag SlotTag, bool bAllowStowed);
	bool WieldItem(AEnclaveItem* Item);
	void BuildCycleSlotTags(TArray<FGameplayTag>& OutSlotTags) const;
	void RebuildInputFilters();
	bool HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandleToggleStowedInput(const FEnclaveInputEvent& InputEvent);
	bool HandleItemInput(const FEnclaveInputEvent& InputEvent);
	void ReleaseHeldItemInputs();
	void ReleaseHeldCharacterInputs();
	void ForwardCharacterActionInput(const FEnclaveInputEvent& InputEvent, const FGameplayTag& CharacterInputTag) const;

	/** Find the wielded item's ability spec with the requested dynamic input tag. */
	FGameplayAbilitySpecHandle FindWieldedItemAbilityForInput(const FGameplayTag& InputTag) const;
	bool TryWieldInDirection(int32 Direction);
	bool IsAvailableWieldable(const AEnclaveItem* Item) const;

	/** The owner's avatar-presentation implementation, discovered by interface. */
	IEnclaveAvatarPresentationInterface* GetAvatarPresentation() const;
	void UpdateWieldedItemActorVisibility(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem);
	void UpdateWieldedItemPresentation(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem);
	void BeginCurrentWieldedItemEquipWindow(AEnclaveItem* Item);
	void ClearCurrentWieldedItemEquipWindow();
	void CancelActiveItemAbilities();
	void GrantAbilitiesForItem(AEnclaveItem* Item);
	void RemoveGrantedAbilities();
	void ApplyWieldedEffects(AEnclaveItem* Item);
	void RemoveWieldedEffects();

	// --- Cross-domain coordination: assignments notify wielding, not vice versa ---
	/** Called after an assignment disappears; maintains the active-selection invariant. */
	void ReconcileSelectionAfterSlotChange();

	// --- Shared GAS and owner lifecycle infrastructure ---
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	void SetManagedAbilitySystem(UAbilitySystemComponent* AbilitySystemComponent);
	void ApplyItemEffects(AEnclaveItem* Item, const TArray<TSubclassOf<UGameplayEffect>>& EffectClasses,
		const TCHAR* EffectCategory, bool bRequireIndependentInstances,
		TArray<FActiveGameplayEffectHandle>& OutHandles);
	void RemoveEffectHandles(TArray<FActiveGameplayEffectHandle>& Handles);

	/** Retained because the PlayerState ASC may outlive the Pawn and its pointers. */
	TWeakObjectPtr<UAbilitySystemComponent> ManagedAbilitySystem;
	bool bEquipmentTearingDown = false;

	// Non-replicated domain state. Replicated fields remain on this component.
	FEnclaveEquipmentAssignmentRuntimeState AssignmentState;
	FEnclaveEquipmentWieldRuntimeState WieldState;

	// --- Reflected configuration and replicated state ---
	/** Ordinary slots only (armor, weapons, gadgets), in wield-cycle order.
	 * Default and Stowed are reserved and must not be authored here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Slots", meta = (AllowPrivateAccess = "true"))
	TArray<FEnclaveEquipmentSlotDefinition> SlotDefinitions;

	/** Default is always wieldable; its participation in cycling is configurable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Reserved Slots", meta = (AllowPrivateAccess = "true"))
	bool bDefaultInWieldCycle = true;

	/** Replicated slot assignments; item lifetimes remain with their owners. */
	UPROPERTY(ReplicatedUsing = OnRep_SlotAssignments)
	TArray<FEnclaveEquipmentSlotAssignment> SlotAssignments;

	UFUNCTION()
	void OnRep_SlotAssignments();

	UPROPERTY(Replicated)
	FGameplayTag SelectedSlotTag;

	UFUNCTION()
	void OnRep_CurrentWieldedItem();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWieldedItem)
	TObjectPtr<AEnclaveItem> CurrentWieldedItem = nullptr;

	/** Explicitly assigned source; never discovered by scanning owner components. */
	UPROPERTY(Transient)
	TScriptInterface<IEnclaveItemSourceInterface> ItemSource;

	/** Semantic input tags are designer-configured; cached filters live in WieldState. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment.Wieldable"))
	FGameplayTag NextWieldableInputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment.Wieldable"))
	FGameplayTag PreviousWieldableInputTag;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag ToggleStowedInputTag;
};
