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
 * component can receive damage directly, while a future combatant damage
 * router can implement this contract and resolve shields/armor/resistances
 * before forwarding the remaining damage to health.
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
