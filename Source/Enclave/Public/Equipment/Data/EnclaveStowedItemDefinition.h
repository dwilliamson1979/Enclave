// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Equipment/Data/EnclaveItemDefinition.h"
#include "EnclaveStowedItemDefinition.generated.h"

/** Optional meshless item definition for the Stowed slot. Configure Unstow ability grants in its asset. */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveStowedItemDefinition : public UEnclaveItemDefinition
{
	GENERATED_BODY()

public:
	UEnclaveStowedItemDefinition();
};
