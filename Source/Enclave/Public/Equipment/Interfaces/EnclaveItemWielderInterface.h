// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "EnclaveItemWielderInterface.generated.h"

class AActor;
class AEnclaveItem;

UENUM(BlueprintType)
enum class EEnclaveWieldedItemActionPhase : uint8
{
	/** The action began. Instant actions (Fire, DryFire) only ever send this. */
	Started,

	/** A lasting action (Reload) finished - completed or cancelled alike. */
	Ended
};

/**
 * A wielded item announcing what it is doing - not an instruction for how
 * the wielder should look while it happens. The wielder decides that
 * (montages today; could equally be camera shake, a sound, or nothing).
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveWieldedItemActionEvent
{
	GENERATED_BODY()

public:
	/** What the item is (its GetItemType), e.g. Weapon.Type.Firearm.Rifle. */
	UPROPERTY(BlueprintReadOnly, Category = "Wielder")
	FGameplayTag ItemType;

	/** What it's doing - a Wielder.Action.* tag. */
	UPROPERTY(BlueprintReadOnly, Category = "Wielder")
	FGameplayTag Action;

	UPROPERTY(BlueprintReadOnly, Category = "Wielder")
	EEnclaveWieldedItemActionPhase Phase = EEnclaveWieldedItemActionPhase::Started;

	/** Expected length in seconds when the item knows it (a reload); 0 otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Wielder")
	float Duration = 0.0f;
};

UINTERFACE(MinimalAPI)
class UEnclaveItemWielderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for whatever physically wields Enclave items on an actor -
 * normally a UEnclaveWielderComponent, found the same way as the item
 * source (FindComponentByInterface); an actor implementing it directly also
 * works.
 *
 * Split of responsibility:
 * - UEnclaveEquipmentComponent decides WHAT is wielded (authoritative,
 *   replicated state, granted abilities, input routing, and later any
 *   wield requirements) and tells the wielder when that changes.
 * - The wielder decides HOW that looks on its body: which meshes/views
 *   exist, where the item attaches on each, how it's held, which animation
 *   layers apply, what happens to an item no longer in hand - everything
 *   that depends on the body's skeleton(s).
 * - The item only says what it is (GetItemType) and exposes its own
 *   presentation components.
 *
 * Both calls are local presentation and run on every machine: server from
 * UEnclaveEquipmentComponent::WieldItem, clients from
 * OnRep_CurrentWieldedItem.
 */
class ENCLAVE_API IEnclaveItemWielderInterface
{
	GENERATED_BODY()

public:
	/**
	 * Physically wield Item: attach it, show it, apply this item type's
	 * animation layers. Returns false if this body can't wield it (no
	 * presentation data for its type, a missing socket or mesh) - the
	 * equipment component then rolls back to the previous item. A failed
	 * call leaves Item not visibly wielded.
	 */
	virtual bool WieldItem(AEnclaveItem* Item) = 0;

	/**
	 * Item is no longer the wielded item: remove its animation layers and
	 * put it away (currently hidden in place; a stowed socket later).
	 */
	virtual void UnwieldItem(AEnclaveItem* Item) = 0;

	/**
	 * The wielded item announcing an action (Wielder.Action.*) it is actually
	 * performing - sent only after gameplay has decided it happened (the
	 * item reacts to its gameplay cues, never to raw input). Local
	 * presentation, received on every machine.
	 */
	virtual void HandleWieldedItemAction(const FEnclaveWieldedItemActionEvent& Event) = 0;

	/**
	 * The wielder on Actor: a component implementing this interface (normally
	 * UEnclaveWielderComponent), else Actor itself if it implements it.
	 * Shared lookup for the equipment component (its owner) and items (their
	 * attach parent).
	 */
	static IEnclaveItemWielderInterface* FindWielder(AActor* Actor);
};
