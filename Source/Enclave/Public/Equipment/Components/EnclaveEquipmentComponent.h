// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "Inventory/Interfaces/EnclaveItemSourceInterface.h"
#include "GameplayAbilitySpecHandle.h"
#include "EnclaveEquipmentComponent.generated.h"

class AEnclaveItem;
class UAbilitySystemComponent;

/**
 * Manages the character's currently wielded item and item-action input.
 *
 * This component owns wielding operations, including selecting the next or
 * previous wieldable item from an item source and presenting the active item.
 *
 * It also routes generic item actions (Primary, Secondary, Tertiary) to the
 * currently wielded item.
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
	AEnclaveItem* GetCurrentWieldedItem() const { return CurrentWieldedItem; }

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	bool TryWieldNext();
	bool TryWieldPrevious();
	bool CanCycleWieldables() const;

	bool TryPerformItemPrimary(const FEnclaveInputEvent& InputEvent);
	bool TryPerformItemSecondary(const FEnclaveInputEvent& InputEvent);
	bool TryPerformItemTertiary(const FEnclaveInputEvent& InputEvent);

private:
	void RebuildInputFilters();

	bool HandleNextWieldableInput(const FEnclaveInputEvent& InputEvent);
	bool HandlePreviousWieldableInput(const FEnclaveInputEvent& InputEvent);

	bool HandleItemPrimaryInput(const FEnclaveInputEvent& InputEvent);
	bool HandleItemSecondaryInput(const FEnclaveInputEvent& InputEvent);
	bool HandleItemTertiaryInput(const FEnclaveInputEvent& InputEvent);

	bool TryWieldInDirection(int32 Direction);
	bool IsAvailableWieldable(const AEnclaveItem* Item) const;

	// Item source binding
	void BindItemSource();
	void UnbindItemSource();
	void HandleItemSourceChanged();
	void RebuildWieldableCache();

	bool AttachItemToOwner(AEnclaveItem* Item);
	void SetWieldedPresentation(AEnclaveItem* Item, bool bWielded);

	// Ability granting - server-authoritative only, mirrors the existing
	// authority gating on WieldItem. Clients learn about granted abilities
	// through the ASC's own replication, not through this path.
	UAbilitySystemComponent* GetOwnerAbilitySystemComponent() const;
	void GrantAbilitiesForItem(AEnclaveItem* Item);
	void RemoveGrantedAbilities();

	/** Handles for abilities currently granted for CurrentWieldedItem. Server-only bookkeeping, not replicated. */
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;

	UFUNCTION(Server, Reliable)
	void ServerTryWieldNext();

	UFUNCTION(Server, Reliable)
	void ServerTryWieldPrevious();

	/** Semantic input tag used to request the next available wieldable item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag NextWieldableInputTag;

	/** Semantic input tag used to request the previous available wieldable item. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Equipment"))
	FGameplayTag PreviousWieldableInputTag;

	/** Semantic input tag used for the currently wielded item's primary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag ItemPrimaryActionInputTag;

	/** Semantic input tag used for the currently wielded item's secondary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag ItemSecondaryActionInputTag;

	/** Semantic input tag used for the currently wielded item's tertiary action. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input.Item"))
	FGameplayTag ItemTertiaryActionInputTag;

	/** Cached routing filters derived from the configured input tags. */
	FGameplayTagContainer InputFilters;

	UFUNCTION()
	void OnRep_CurrentWieldedItem();

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWieldedItem)
	TObjectPtr<AEnclaveItem> CurrentWieldedItem = nullptr;

	/** Local presentation cache used to clean up the previously wielded item on replication. */
	TWeakObjectPtr<AEnclaveItem> PresentedWieldedItem;

	/** The owner's bound item source (e.g. an inventory component), accessed only through the interface. */
	UPROPERTY(Transient)
	TScriptInterface<IEnclaveItemSourceInterface> ItemSource;

	/** Subset of the item source's items that implement IEnclaveWieldableInterface. Rebuilt only on source-change notifications. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AEnclaveItem>> WieldableCache;
};
