// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Gameplay Cue tags - Phase 3.
 *
 * These drive purely cosmetic responses (montage, muzzle flash, fire sound)
 * triggered off an ability's activation without that ability knowing
 * anything about what, if anything, responds to the cue. See
 * UEnclaveGA_Fire (trigger side) and UEnclaveGameplayCueNotify /
 * IEnclaveGameplayCueResponderInterface (response side - a Blueprint child
 * of UEnclaveGameplayCueNotify per tag forwards to whatever implements the
 * interface, e.g. AEnclaveWeapon).
 */
namespace EnclaveGameplayTags
{
	/** Executed by UEnclaveGA_Fire on every successful activation, regardless of whether the trace hits anything. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);

	/** Executed by a reload ability on activation - reload montage/sound, no muzzle flash. Not yet wired to an ability; added for testing the cue pipeline ahead of the reload ability itself. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Reload);
}
