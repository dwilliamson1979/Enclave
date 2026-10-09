// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Effects/EnclaveGameplayEffect.h"
#include "EnclaveGE_Damage.generated.h"

/**
 * Native instant Gameplay Effect used by the generic GAS health receiver.
 *
 * The caller supplies a positive Data.Damage magnitude. The effect adds that
 * amount to UEnclaveAttributeSet::Damage; the Attribute Set consumes the meta
 * attribute and converts it into Health loss in PostGameplayEffectExecute.
 */
UCLASS()
class ENCLAVE_API UEnclaveGE_Damage : public UEnclaveGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclaveGE_Damage();
};
