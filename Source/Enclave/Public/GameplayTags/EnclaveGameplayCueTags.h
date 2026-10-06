// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Gameplay Cue tags - Phase 3.
 *
 * These drive purely cosmetic responses (avatar/item montages, muzzle flash,
 * fire sound) triggered off an ability's activation without that ability
 * knowing how they are rendered. Each concrete UEnclaveGameplayCueNotify
 * Blueprint configures the semantic Presentation.Item.Action.* mapping and
 * cue lifecycle mode in Class Defaults; AvatarPresentation coordinates both
 * avatar and item-local presentation.
 */
namespace EnclaveGameplayTags
{
	/** Root for semantic item-action Gameplay Cues emitted by item abilities. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Item_Action);

	/** Executed by UEnclaveGA_Fire on every successful activation, regardless of whether the trace hits anything. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Item_Action_Fire);

	/**
	 * Added (not executed) by UEnclaveGA_Reload for the duration of the
	 * reload and removed automatically when the ability ends - completed OR
	 * cancelled - so the responder gets a WhileActive to start its reload
	 * presentation and a Removed to stop it. The project's first duration
	 * cue; Fire/DryFire stay one-shot executes.
	 */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Item_Action_Reload);

	/** Executed by UEnclaveGA_Fire when activated with an empty magazine - dry-fire click, nothing else. No cooldown is committed and nothing traces. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Item_Action_DryFire);
}