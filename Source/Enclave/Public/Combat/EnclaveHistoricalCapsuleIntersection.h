// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"

struct FEnclaveTargetCapsuleFrame;

/**
 * Pure historical geometry query: segment vs an oriented, capped cylinder.
 * Does not depend on scene collision, mutate an actor, or authorize damage.
 * Returns distance from Start to the earliest intersection in [Start, End].
 */
struct ENCLAVE_API FEnclaveHistoricalCapsuleIntersection
{
	static bool TryIntersectSegment(const FVector& Start, const FVector& End,
		const FEnclaveTargetCapsuleFrame& Capsule, double& OutDistanceCm, FVector& OutPoint);
};
