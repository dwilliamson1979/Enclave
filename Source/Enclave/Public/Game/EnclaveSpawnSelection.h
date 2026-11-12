// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "EnclaveSpawnSelection.generated.h"

struct FRandomStream;

/** How a match picks where a player spawns. Editor settings on the match GameMode. */
USTRUCT()
struct ENCLAVE_API FEnclaveSpawnRules
{
	GENERATED_BODY()

	/**
	 * Starts whose score is at least this share of the best start's are picked
	 * from at random, so spawns aren't predictable. 1 = always a best start;
	 * 0 = any start.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Spawning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RandomMarginFraction = 0.8f;
};

/** One start a player may spawn at. */
struct FEnclaveSpawnCandidate
{
	FVector Location = FVector::ZeroVector;

	/**
	 * Something overlaps the pawn's capsule here, but the pawn can be nudged
	 * clear. Starts the pawn can't fit at all aren't candidates.
	 */
	bool bBlocked = false;
};

/**
 * Spawn selection as a pure function. It knows nothing about actors or the
 * world: the match GameMode gathers the candidates and the living enemies,
 * calls ChooseStart and spawns at the result, and tests call it with plain data.
 *
 *   1. Blocked starts are used only when every candidate is blocked.
 *   2. A start's score is its distance to the nearest living enemy; with no
 *      enemies, every start scores the same.
 *   3. The pick is random among starts scoring at least RandomMarginFraction
 *      of the best.
 */
namespace EnclaveSpawnSelection
{
	/** The index of the chosen candidate, or INDEX_NONE when there are none. */
	ENCLAVE_API int32 ChooseStart(TConstArrayView<FEnclaveSpawnCandidate> Candidates, TConstArrayView<FVector> EnemyLocations,
		const FEnclaveSpawnRules& Rules, FRandomStream& Random);
}
