#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclaveInputReceiverInterface.generated.h"

UINTERFACE(MinimalAPI)
class UEnclaveInputReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for an object that can receive semantic player input.
 *
 * The PlayerController communicates only with this interface and does not
 * need to know how the receiver internally routes or handles the input.
 *
 * Typical implementations will be controllable Pawns that forward received
 * events into an EnclaveInputRouterComponent.
 */
class ENCLAVE_API IEnclaveInputReceiverInterface
{
	GENERATED_BODY()

public:

	/**
	 * Processes a semantic input event received from the controlling
	 * PlayerController.
	 */
	virtual void ProcessInput(const FEnclaveInputEvent& InputEvent) = 0;
};