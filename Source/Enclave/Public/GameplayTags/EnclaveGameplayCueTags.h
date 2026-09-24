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

	/**
	 * Added (not executed) by UEnclaveGA_Reload for the duration of the
	 * reload and removed automatically when the ability ends - completed OR
	 * cancelled - so the responder gets a WhileActive to start its reload
	 * presentation and a Removed to stop it. The project's first duration
	 * cue; Fire/DryFire stay one-shot executes.
	 */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_Reload);

	/** Executed by UEnclaveGA_Fire when activated with an empty magazine - dry-fire click, nothing else. No cooldown is committed and nothing traces. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Weapon_DryFire);
}
