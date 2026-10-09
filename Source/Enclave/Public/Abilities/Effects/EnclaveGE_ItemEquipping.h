// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Effects/EnclaveGameplayEffect.h"
#include "EnclaveGE_ItemEquipping.generated.h"

/**
 * Native duration effect representing the authoritative equip window for the
 * current wield relationship.
 *
 * The applied spec supplies the actual duration at runtime. While active this
 * effect grants State.Item.Equipping for observable state and blocks
 * Ability.Type.Action.Item through GAS for the same authoritative window.
 * Equipment never needs to know any concrete item-action ability types.
 */
UCLASS()
class ENCLAVE_API UEnclaveGE_ItemEquipping : public UEnclaveGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclaveGE_ItemEquipping();
};
