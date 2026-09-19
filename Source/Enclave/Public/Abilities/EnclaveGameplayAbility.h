// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnclaveGameplayAbility.generated.h"

/**
 * Project base class for all Enclave Gameplay Abilities.
 *
 * Deliberately thin for now - it exists so every ability shares one place to
 * add common behavior later (cost/cooldown conventions, a shared way to read
 * the source item, etc.) rather than needing a mechanical find-and-replace
 * across every ability subclass once that need shows up.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGameplayAbility();
};
