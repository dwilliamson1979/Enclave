// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Combat/Interfaces/EnclaveDamagePolicyInterface.h"
#include "EnclaveHitPolicy.generated.h"

/**
 * How one match phase (warmup, match, post-match) treats hits on players, by
 * how the attacker regards the victim. Hits on non-players (dummies,
 * destructibles) are always applied: what damage does to them is their own
 * code's business. Hits on neutral players (spectators, players with no team)
 * are always ignored.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveHitPolicy
{
	GENERATED_BODY()

	/** An enemy player. */
	UPROPERTY(EditDefaultsOnly, Category = "Hit Policy")
	EEnclaveHitResponse Hostile = EEnclaveHitResponse::Apply;

	/** A teammate (friendly fire). Never happens in a free-for-all. */
	UPROPERTY(EditDefaultsOnly, Category = "Hit Policy")
	EEnclaveHitResponse Friendly = EEnclaveHitResponse::Ignore;

	/** Your own damage (your grenade). */
	UPROPERTY(EditDefaultsOnly, Category = "Hit Policy")
	EEnclaveHitResponse Self = EEnclaveHitResponse::Apply;

	/** Damage with no attacker (falling, fire). */
	UPROPERTY(EditDefaultsOnly, Category = "Hit Policy")
	EEnclaveHitResponse Environment = EEnclaveHitResponse::Apply;

	/** Every hit on a player gets Response; e.g. Simulated() for warmup. */
	static FEnclaveHitPolicy All(EEnclaveHitResponse Response)
	{
		FEnclaveHitPolicy Policy;
		Policy.Hostile = Response;
		Policy.Friendly = Response;
		Policy.Self = Response;
		Policy.Environment = Response;
		return Policy;
	}

	/** Players see every hit, but nobody gets hurt. */
	static FEnclaveHitPolicy Simulated() { return All(EEnclaveHitResponse::Simulate); }
};
