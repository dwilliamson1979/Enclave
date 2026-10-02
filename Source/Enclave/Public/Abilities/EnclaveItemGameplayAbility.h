// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveItemGameplayAbility.generated.h"

/**
 * Base class for Gameplay Abilities granted by the currently wielded item.
 *
 * Item abilities share one readiness rule: they cannot activate while the
 * current wield relationship owns State.Item.Wielded.Equipping. Equipment
 * establishes that state through GAS; individual abilities remain responsible
 * for their own additional action-specific activation rules.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveItemGameplayAbility : public UEnclaveGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveItemGameplayAbility();
};
