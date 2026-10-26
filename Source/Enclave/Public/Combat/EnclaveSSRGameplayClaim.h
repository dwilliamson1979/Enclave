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
	// D1.1C scenario tag: 0=unassigned, 1=stationary, 2=strafe, 3=reverse, 4=crouch, 5=cover.
	// Diagnostic-only untrusted label, never gameplay authority.
	uint8 Scenario = 0;
	TWeakObjectPtr<AActor> Target;
	double EchoedMovementStamp = 0.0;
	float SecondsSinceMovementUpdate = 0.0f;
};
