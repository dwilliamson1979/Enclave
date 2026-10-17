// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Equipment/Data/EnclaveItemDefinition.h"
#include "EnclaveBasicItemDefinition.generated.h"

/** Concrete generic definition for equipment that needs no specialized weapon or
 * Stowed behavior. No capabilities are implicitly granted; designers configure
 * slot requirements and EffectsWhileEquipped for armor, utility items, etc. */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveBasicItemDefinition : public UEnclaveItemDefinition
{
	GENERATED_BODY()
};
