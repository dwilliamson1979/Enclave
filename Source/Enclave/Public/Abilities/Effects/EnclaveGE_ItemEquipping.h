// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EnclaveGE_ItemEquipping.generated.h"

/**
 * Native duration effect representing the authoritative equip window for the
 * current wield relationship.
 *
 * The applied spec supplies the actual duration at runtime. While active this
 * effect grants State.Item.Wielded.Equipping to the owner's ASC, allowing GAS
 * ability activation rules to enforce readiness without Equipment knowing any
 * concrete item-action ability types.
 */
UCLASS()
class ENCLAVE_API UEnclaveGE_ItemEquipping : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclaveGE_ItemEquipping();
};
