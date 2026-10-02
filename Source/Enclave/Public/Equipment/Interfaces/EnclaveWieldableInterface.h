// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveWieldableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveWieldableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Marker capability for items that participate in Enclave's wield/equipment
 * system.
 *
 * This interface intentionally exposes no item-action API. Support for
 * Input.Item.Action.* is defined by the abilities granted for the currently
 * wielded item and resolved through GAS by UEnclaveEquipmentComponent.
 */
class ENCLAVE_API IEnclaveWieldableInterface
{
	GENERATED_BODY()
};
