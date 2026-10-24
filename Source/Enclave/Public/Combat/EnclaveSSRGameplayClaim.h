// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"

class AActor;

/** Transient opt-in test oracle carried inside existing GAS shot TargetData.
 * Never trusted for hit registration or damage. Not a production wire protocol.
 */
struct FEnclaveSSRGameplayClaim
{
	// 0=no sample, 1=clear hit, 2=clear miss, 3=near miss (10cm geometric margin).
	uint8 Classification = 0;
	TWeakObjectPtr<AActor> Target;
	float EchoedMovementStamp = 0.0f;
	float SecondsSinceMovementUpdate = 0.0f;
};
