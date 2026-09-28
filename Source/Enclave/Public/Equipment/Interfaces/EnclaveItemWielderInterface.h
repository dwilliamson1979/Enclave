// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveItemWielderInterface.generated.h"

class AEnclaveItem;

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
};
