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
 * UEnclaveGA_Fire (trigger side) and UEnclaveGameplayCueNotify_WeaponFire
 * (response side).
 */
namespace EnclaveGameplayTags
{
	/** Executed by UEnclaveGA_Fire on every successful activation, regardless of whether the trace hits anything. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Fire);
}
