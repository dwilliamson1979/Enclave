// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveHitFeedbackInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveHitFeedbackInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by whatever currently hosts a local player's combat feedback
 * (today, UEnclaveCombatFeedbackComponent on AEnclavePlayerController).
 *
 * Lets ability code - or anything else - notify the local player of combat
 * events without knowing or caring what actually shows them. Same shape as
 * IEnclaveItemSourceInterface/IEnclaveInputReceiverInterface elsewhere in
 * the project: depend on the interface, find the implementer with
 * FindComponentByInterface, never cast to a concrete class.
 */
class ENCLAVE_API IEnclaveHitFeedbackInterface
{
	GENERATED_BODY()

public:
	/**
	 * The local player's last shot landed a hit and applied damage.
	 * Typically called from server-side ability code (which has no direct
	 * connection to the owning client) - implementations are responsible
	 * for getting the notification to the owning client themselves (e.g.
	 * via a Client RPC) if they aren't already running there.
	 */
	virtual void NotifyWeaponHitConfirmed() = 0;
};
