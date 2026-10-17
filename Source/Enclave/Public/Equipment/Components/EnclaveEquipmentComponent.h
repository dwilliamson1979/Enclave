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

	/** Whether this slot supports active wield selection (including Stowed).
	 * Existing serialized Blueprint slots with bInWieldCycle=true remain
	 * wieldable even when this newly introduced flag defaults to false. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	bool bCanWield = false;

	/** Whether an eligible wieldable participates in Next/Previous. Also implies wield eligibility. */
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

/** Tracks one validated slot occupant and precisely the passive effects granted for it. */
struct FEnclaveEquippedItemEffectState
{
	TWeakObjectPtr<AEnclaveItem> Item;
	TArray<FActiveGameplayEffectHandle> EffectHandles;
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

	// UActorComponent
	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void UninitializeComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	UFUNCTION(BlueprintCallable, Category = "Equipment|Slots")
	bool SelectSlot(FGameplayTag SlotTag);

	UFUNCTION(BlueprintPure, Category = "Equipment|Slots")
	AEnclaveItem* GetItemInSlot(FGameplayTag SlotTag) const;

	const TArray<FEnclaveEquipmentSlotAssignment>& GetSlotAssignments() const { return SlotAssignments; }

	/** Server-only reconciliation when the owner first binds (or rebinds) its ASC.
	 * Called automatically on assignment changes; harmless if the ASC is absent. */
	void RefreshEquippedEffects();

	/**
	 * Authority-only teardown of active wielding/selection. Cancels item actions,
	 * revokes grants, clears equip readiness and presentation. Does not destroy items;
	 * their lifetime belongs to the item source or an external owner.
	 */
	void ClearCurrentWieldedItem();

	AEnclaveItem* GetCurrentWieldedItem() const { return CurrentWieldedItem; }

	/**
	 * Derived readiness query. GAS is the single source of truth: an item is
	 * actionable only when one is wielded and State.Item.Equipping is absent.
	 */
	bool IsCurrentWieldedItemReadyForActions() const;

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual EEnclaveInputNetworkPolicy GetInputNetworkPolicy(const FEnclaveInputEvent& InputEvent) const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	bool TryWieldNext();
	bool TryWieldPrevious();
	bool CanCycleWieldables() const;

	/** Server-authoritative toggle; has no effect if Stowed is not configured and assigned. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Stowed")
	bool ToggleStowed();

	/** Called by an item's Unstow ability on the server. Falls back to Default if return slot is empty. */
	UFUNCTION(BlueprintCallable, Category = "Equipment|Stowed")
	bool RestoreStowedSelection();

	/** Migration-only fallback for older Character Blueprints which configured Stowed on Equipment. */
	TSubclassOf<AEnclaveItem> GetLegacyStowedItemClass() const { return StowedItemClass; }

private:
	/** Selects any normally accessible slot. Stowed requires the dedicated toggle entry path. */
	bool SelectSlotInternal(FGameplayTag SlotTag, bool bAllowStowed);
	bool WieldItem(AEnclaveItem* Item);
	void BuildCycleSlotTags(TArray<FGameplayTag>& OutSlotTags) const;
	void ValidateSlotDefinitions() const;
	void RebuildInputFilters();

	bool HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandleToggleStowedInput(const FEnclaveInputEvent& InputEvent);

	bool HandleItemInput(const FEnclaveInputEvent& InputEvent);

	/** Clears local held-input bookkeeping; revoked specs are safe to release later. */
	void ReleaseHeldItemInputs();

	/** A press owns its resolved spec until release, even if CurrentWieldedItem changes. */
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> HeldItemInputSpecs;

	/**
	 * The granted ability spec (on the owner's ASC) whose SourceObject is
	 * CurrentWieldedItem and whose dynamic spec source tags contain
	 * InputTag - i.e. "the ability this item bound to this input". Invalid
	 * handle if the item granted nothing for InputTag. Works on the owning
	 * client too: activatable ability specs, including SourceObject and
	 * dynamic tags, replicate to the owner.
	 */
	FGameplayAbilitySpecHandle FindWieldedItemAbilityForInput(const FGameplayTag& InputTag) const;

	bool TryWieldInDirection(int32 Direction);
	bool IsAvailableWieldable(const AEnclaveItem* Item) const;
	bool IsAssignmentValid(const FEnclaveEquipmentSlotAssignment& Assignment) const;
	bool AssignItemToSlot(FGameplayTag SlotTag, AEnclaveItem* Item, bool bFromItemSource);
	const FEnclaveEquipmentSlotDefinition* FindSlotDefinition(FGameplayTag SlotTag) const;
	const FEnclaveEquipmentSlotAssignment* FindSlotAssignment(FGameplayTag SlotTag) const;
	void ReconcileSelectionAfterSlotChange();
	void UnbindItemSource();
	void HandleItemSourceChanged();

	/** An external owner may destroy its item independently of its slot. */
	UFUNCTION()
	void HandleAssignedItemDestroyed(AActor* DestroyedActor);

	/** The owner's avatar-presentation implementation, discovered by interface. */
	IEnclaveAvatarPresentationInterface* GetAvatarPresentation() const;

	/** Applies authoritative actor visibility for the wield transition. */
	void UpdateWieldedItemActorVisibility(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem);

	/** Reports an already-established wielded-item state change to avatar presentation. */
	void UpdateWieldedItemPresentation(AEnclaveItem* PreviousItem, AEnclaveItem* NewItem);

	// Gameplay-authoritative equip window for CurrentWieldedItem. GAS owns both
	// the clock and the State.Item.Equipping tag that blocks item actions.
	void BeginCurrentWieldedItemEquipWindow(AEnclaveItem* Item);
	void ClearCurrentWieldedItemEquipWindow();

	// Ability granting - server-authoritative only, mirrors the existing
	// authority gating on WieldItem. Clients learn about granted abilities
	// through the ASC's own replication, not through this path.
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	void CancelActiveItemAbilities();
	void GrantAbilitiesForItem(AEnclaveItem* Item);
	void RemoveGrantedAbilities();
	void ApplyWieldedEffects(AEnclaveItem* Item);
	void RemoveWieldedEffects();
	void ApplyItemEffects(AEnclaveItem* Item, const TArray<TSubclassOf<UGameplayEffect>>& EffectClasses,
		const TCHAR* EffectCategory, bool bRequireIndependentInstances,
		TArray<FActiveGameplayEffectHandle>& OutHandles);
	void RemoveEffectHandles(TArray<FActiveGameplayEffectHandle>& Handles);
	void RemoveAllEquippedEffects();

	/**
	 * The ASC that owns granted abilities, equip readiness, wielded effects, and
	 * effects attached to equipped slot assignments.
	 *
	 * Stored explicitly because pawn teardown can clear the pawn's PlayerState before
	 * UninitializeComponent runs. Cleanup must revoke the exact GAS state this
	 * Equipment instance created without rediscovering the ASC through its owner.
	 */
	TWeakObjectPtr<UAbilitySystemComponent> ManagedAbilitySystem;

	/** Handles for abilities currently granted for CurrentWieldedItem. Server-only bookkeeping, not replicated. */
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	/** Server-only effect handles, owned by this Equipment instance. */
	TArray<FActiveGameplayEffectHandle> WieldedEffectHandles;

	/** Authority-only effects tracked per assignment, independent of CurrentWieldedItem.
	 * Entries with no authored effects are kept to avoid repeat applications. */
	TMap<FGameplayTag, FEnclaveEquippedItemEffectState> EquippedEffectStates;

	/** Semantic input tag used to request the next available wieldable item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment.Wieldable"))
	FGameplayTag NextWieldableInputTag;

	/** Semantic input tag used to request the previous available wieldable item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment.Wieldable"))
	FGameplayTag PreviousWieldableInputTag;

	/** Discrete Equipment command; H key is mapped in the owning InputConfig asset. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag ToggleStowedInputTag;

	/** Cached routing filters derived from the configured input tags. */
	FGameplayTagContainer InputFilters;

	UFUNCTION()
	void OnRep_CurrentWieldedItem();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWieldedItem)
	TObjectPtr<AEnclaveItem> CurrentWieldedItem = nullptr;

	/** Authority-only handle for the duration Gameplay Effect that owns the current equip window. */
	FActiveGameplayEffectHandle CurrentWieldedItemEquipEffectHandle;

	/** The wielded item whose presentation has most recently been reconciled on this machine. */
	TWeakObjectPtr<AEnclaveItem> LocallyWieldedItem;

	/** Ordinary slots only (armor, weapons, gadgets), in wield-cycle order.
	 * Legacy serialized reserved rows retain authored restrictions until migrated. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Slots", meta = (AllowPrivateAccess = "true"))
	TArray<FEnclaveEquipmentSlotDefinition> SlotDefinitions;

	/** Default is a reserved, always-wieldable slot; it may participate in cycling. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Reserved Slots", meta = (AllowPrivateAccess = "true"))
	bool bDefaultInWieldCycle = true;

	/** Legacy Blueprint setting. New archetypes configure StowedItemClass on Character. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Legacy", meta = (AllowPrivateAccess = "true", DisplayName = "Legacy Stowed Item Class"))
	TSubclassOf<AEnclaveItem> StowedItemClass;

	/** Shared runtime definitions for the two reserved slot identities. */
	FEnclaveEquipmentSlotDefinition DefaultReservedDefinition;
	FEnclaveEquipmentSlotDefinition StowedReservedDefinition;

	/** Fallback selection; defaults to the reserved Default slot. Kept for existing Blueprint compatibility. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Slots", meta = (AllowPrivateAccess = "true", Categories = "Equipment.Slot"))
	FGameplayTag DefaultSelectionSlotTag;

	/** Replicated assignment metadata; references do not confer lifetime ownership. */
	UPROPERTY(ReplicatedUsing = OnRep_SlotAssignments)
	TArray<FEnclaveEquipmentSlotAssignment> SlotAssignments;

	UFUNCTION()
	void OnRep_SlotAssignments();

	/** Slot selected for the active wield relationship; cleared by full teardown. */
	UPROPERTY(Replicated)
	FGameplayTag SelectedSlotTag;

	/** Authority-only, one-value restore target. Stale/cleared slots fall back to Default. */
	FGameplayTag ReturnSlotTag;

	/** Guards fallback logic during component/owner shutdown. */
	bool bEquipmentTearingDown = false;

	/** Explicitly assigned source; never discovered by scanning owner components. */
	UPROPERTY(Transient)
	TScriptInterface<IEnclaveItemSourceInterface> ItemSource;
};
