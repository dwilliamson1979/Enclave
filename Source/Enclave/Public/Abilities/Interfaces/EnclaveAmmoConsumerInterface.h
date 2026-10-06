// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayPrediction.h"
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
 * three), so no method takes an amount.
 *
 * Client-predicted: the owning client spends ammo locally the moment it
 * predicts a shot, and the server reconciles. Every method that changes or
 * settles ammo takes the activation's FPredictionKey - the one identifier
 * the predicting client and the server share for "this particular shot" -
 * so the implementer can match the server's answer to the client's guess.
 */
class ENCLAVE_API IEnclaveAmmoConsumerInterface
{
	GENERATED_BODY()

public:
	/** True if one activation's worth of ammo is available right now. Called on both the predicting client and the server - on the owning client this must read the *predicted* count, or the client will predict shots past its last round. */
	virtual bool CanConsumeAmmo() const = 0;

	/**
	 * Spends one activation's worth of ammo. Called on BOTH sides from
	 * UEnclaveGA_Fire::ApplyCost: on the server it is the real spend (and
	 * acknowledges PredictionKey); on the predicting client it records a
	 * predicted spend under PredictionKey, to be settled when the server's
	 * acknowledgement or a rejection of that key arrives.
	 */
	virtual void ConsumeAmmo(const FPredictionKey& PredictionKey) = 0;

	/**
	 * Server only: this activation reached the server but spent no ammo
	 * (dry fire, failed commit). Acknowledging it anyway is what lets the
	 * owning client drop a spend it predicted for this key - without it, a
	 * client that disagreed with the server would stay one round short
	 * until its next shot.
	 */
	virtual void AcknowledgeAmmoPrediction(const FPredictionKey& PredictionKey) = 0;
};
