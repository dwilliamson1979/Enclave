#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclaveAvatarInputPolicyInterface.generated.h"

UINTERFACE(MinimalAPI)
class UEnclaveAvatarInputPolicyInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Policy contract for whether a controllable avatar may currently process a
 * semantic player-input event.
 *
 * The input framework deliberately asks this question without knowing why an
 * avatar may be blocked. Lifecycle, stun, cinematics, possession state, or
 * future game-specific rules remain implementation details of the avatar.
 *
 * The same policy is evaluated on the owning client for responsiveness and on
 * the server before any client-originated server-authoritative input is routed.
 */
class ENCLAVE_API IEnclaveAvatarInputPolicyInterface
{
	GENERATED_BODY()

public:
	virtual bool CanProcessAvatarInput(const FEnclaveInputEvent& InputEvent) const = 0;
};
