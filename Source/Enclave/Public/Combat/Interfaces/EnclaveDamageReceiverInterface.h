// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveDamageReceiverInterface.generated.h"

struct FEnclaveDamageRequest;

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveDamageReceiverInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by the single top-level component on an Actor that accepts
 * Enclave damage requests.
 *
 * The interface is intentionally independent of health. A simple health
 * component receives damage directly. A GAS combatant's receiver (today
 * UEnclaveGASHealthComponent) applies the request as a gameplay effect:
 * damage pools (health, later shields and armor) are attributes drained in a
 * fixed order where the Damage meta-attribute is consumed, and temporary
 * shields or absorbs are gameplay effects. Pools are not separate components
 * registered with the receiver.
 */
class ENCLAVE_API IEnclaveDamageReceiverInterface
{
	GENERATED_BODY()

public:
	/**
	 * Attempts to process a server-authoritative damage request.
	 * Returns true only when this receiver accepted and applied the request.
	 */
	virtual bool ReceiveDamage(const FEnclaveDamageRequest& DamageRequest) = 0;
};
