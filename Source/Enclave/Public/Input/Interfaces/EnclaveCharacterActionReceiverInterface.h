// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclaveCharacterActionReceiverInterface.generated.h"

UINTERFACE(MinimalAPI)
class UEnclaveCharacterActionReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Receives semantic character-owned input explicitly delegated by a wielded
 * item's CharacterFallback binding. The event's InputTag is the configured
 * CharacterInputTag, not the originating physical item-action tag.
 *
 * This is intentionally not an IEnclaveInputHandlerInterface registration:
 * Equipment has already decided who owns the physical input. Dispatching
 * directly prevents a denied item ability from falling through the router.
 * The receiver owns its action implementation and eventual GAS spec lifetime.
 */
class ENCLAVE_API IEnclaveCharacterActionReceiverInterface
{
	GENERATED_BODY()

public:
	virtual void HandleCharacterActionInput(const FEnclaveInputEvent& InputEvent) = 0;
};
