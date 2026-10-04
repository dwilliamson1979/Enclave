// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "Inventory/Interfaces/EnclaveItemSourceInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "ActiveGameplayEffectHandle.h"
#include "EnclaveEquipmentComponent.generated.h"

class AEnclaveItem;
class UAbilitySystemComponent;
class IEnclaveAvatarPresentationInterface;

/**
 * Manages the character's currently wielded item and item-action input.
 *
 * This component owns wielding operations, including selecting the next or
 * previous wieldable item from an item source and notifying the owner's avatar presentation
 * to visually present the active item.
 *
 * It also bridges generic Input.Item.Action.* input to Gameplay Ability
 * System abilities granted by the currently wielded item. The input tag on
 * the event is matched against the dynamic source tag carried by each granted
 * FEnclaveAbilityGrant spec; the ability itself owns action-specific gameplay
 * validation and behavior.
 *
 * Equipment depends only on IEnclaveItemSourceInterface, not on any concrete
 * storage component. It binds to the owner's item source once, caches the
 * subset of exposed items that are wieldable, and refreshes that cache only
 * when the source reports a change - it never polls the source directly.
 *
 * Inventory/storage responsibilities do not belong here.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveEquipmentComponent : public UActorComponent, public IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:
	UEnclaveEquipmentComponent();

	// UActorComponent
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool WieldItem(AEnclaveItem* Item);

	/**
	 * Authority-only teardown of the current wield relationship. Cancels active
	 * item actions, revokes item-granted abilities, removes the equip window,
	 * clears replicated wield state, and reconciles avatar presentation.
	 *
	 * Intentionally does not destroy inventory items; item lifetime belongs to
	 * the item source (currently UEnclaveInventoryComponent).
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
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	bool TryWieldNext();
	bool TryWieldPrevious();
	bool CanCycleWieldables() const;

private:
	void RebuildInputFilters();

	bool HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent);

	bool HandleItemInput(const FEnclaveInputEvent& InputEvent);

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

	// Item source binding
	void BindItemSource();
	void UnbindItemSource();
	void HandleItemSourceChanged();
	void RebuildWieldableCache();

	/** The owner's avatar-presentation implementation, discovered by interface. */
	IEnclaveAvatarPresentationInterface* GetAvatarPresentation() const;

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

	/** Handles for abilities currently granted for CurrentWieldedItem. Server-only bookkeeping, not replicated. */
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UFUNCTION(Server, Reliable)
	void ServerTryWieldNext();

	UFUNCTION(Server, Reliable)
	void ServerTryWieldPrevious();

	/** Semantic input tag used to request the next available wieldable item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment.Wieldable"))
	FGameplayTag NextWieldableInputTag;

	/** Semantic input tag used to request the previous available wieldable item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment.Wieldable"))
	FGameplayTag PreviousWieldableInputTag;

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

	/** The owner's bound item source (e.g. an inventory component), accessed only through the interface. */
	UPROPERTY(Transient)
	TScriptInterface<IEnclaveItemSourceInterface> ItemSource;

	/** Subset of the item source's items that declare Item.Capability.Wieldable. Rebuilt only on source-change notifications. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnclaveItem>> WieldableCache;
};
