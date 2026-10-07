// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EnclaveGE_Dead.generated.h"

/**
 * Persistent GAS representation of the avatar's Dead lifecycle state.
 *
 * While active this effect grants State.Lifecycle.Dead, immediately cancels
 * every active ability classified under Ability.Type.Action, and blocks new
 * abilities in that same action family from activating. It deliberately does
 * not tear down equipment or other owned systems; semantic interruption and
 * ownership cleanup remain separate concerns.
 */
UCLASS()
class ENCLAVE_API UEnclaveGE_Dead : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclaveGE_Dead();
};
