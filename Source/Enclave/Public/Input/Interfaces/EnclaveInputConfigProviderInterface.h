#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveInputConfigProviderInterface.generated.h"

class UEnclaveInputConfig;

UINTERFACE(MinimalAPI)
class UEnclaveInputConfigProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for controllable objects that provide an Enclave input config.
 *
 * The PlayerController can query the currently possessed Pawn through this
 * interface without depending on a concrete Pawn class such as a soldier,
 * drone, vehicle, turret, or spectator Pawn.
 *
 * The returned config describes the input mapping contexts and semantic
 * action bindings that should become active while controlling this object.
 */
class ENCLAVE_API IEnclaveInputConfigProviderInterface
{
	GENERATED_BODY()

public:

	/**
	 * Returns the input configuration associated with this controllable object.
	 *
	 * Returning nullptr means that this object does not provide an
	 * avatar-specific input configuration.
	 */
	virtual const UEnclaveInputConfig* GetInputConfig() const = 0;
};