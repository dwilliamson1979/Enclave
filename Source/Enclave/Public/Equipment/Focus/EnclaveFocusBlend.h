// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"

struct FEnclaveFocusViewSettings;

/**
 * The Focus transition as pure functions, so it can be unit tested.
 *
 * Progress is linear time through the transition (0 hip, 1 aimed); it moves
 * at 1 / EnterDuration per second going in and 1 / ExitDuration coming out. Weight is what the view uses (0 hip,
 * 1 aimed): the settings' easing applied to Progress. Entering uses the easing
 * as authored; leaving mirrors it in time, so an EaseOut leaves fast too.
 */
namespace EnclaveFocusBlend
{
	/** Weight for Progress in the given direction. Both directions agree at 0 and 1. */
	ENCLAVE_API float Weight(float Progress, bool bEntering, const FEnclaveFocusViewSettings& Settings);

	/**
	 * The Progress whose Weight in the given direction is InWeight. Used when the
	 * direction flips mid-blend: entering and leaving map Progress to Weight
	 * differently, so Progress is re-solved and the view doesn't jump.
	 */
	ENCLAVE_API float ProgressForWeight(float InWeight, bool bEntering, const FEnclaveFocusViewSettings& Settings);
}
