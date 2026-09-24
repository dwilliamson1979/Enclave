// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveAmmoConsumerInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveAmmoConsumerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by a triggering source whose activations spend ammunition
 * (AEnclaveFirearmWeapon's magazine today).
 *
 * Optional by design: UEnclaveGA_Fire resolves this via
 * GetTriggeringSourceAs<IEnclaveAmmoConsumerInterface>() and simply applies
 * no ammo cost when the source doesn't implement it - an infinite-ammo
 * sentry or an energy weapon on a heat/charge model just never implements
 * this interface, with no ability-side branching.
 *
 * How much ammo one activation costs is the implementer's business
 * (AmmoPerShot on the weapon definition today; a burst weapon might spend
 * three), so neither method takes an amount.
 */
class ENCLAVE_API IEnclaveAmmoConsumerInterface
{
	GENERATED_BODY()

public:
	/** True if one activation's worth of ammo is available right now. Called on both the predicting client and the server - must only read replicated state. */
	virtual bool CanConsumeAmmo() const = 0;

	/** Spends one activation's worth of ammo. Server-authoritative: UEnclaveGA_Fire only calls this where it has authority; the result reaches the owning client via replication. */
	virtual void ConsumeAmmo() = 0;
};
