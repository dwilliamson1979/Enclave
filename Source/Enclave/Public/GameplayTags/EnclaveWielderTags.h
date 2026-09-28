// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Wielder action tags - what a wielder is visibly DOING with the item in
 * its hands. Carried by FEnclaveWieldedItemActionEvent, and the keys of the
 * per-item-type montage table on UEnclaveItemPresentationDefinition.
 *
 * Deliberately separate from GameplayCue.* (the gameplay events behind
 * them) and Input.Item.* (the buttons pressed): an item translates its own
 * events into these, so a body's data never depends on how a particular
 * item's abilities or cues are named.
 */
namespace EnclaveGameplayTags
{
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wielder_Action);

	/** Item taken in hand. Raised by the wielder itself whenever it wields an item. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wielder_Action_Draw);

	/** One shot fired. Instant (Started only). */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wielder_Action_Fire);

	/** Trigger pulled on an empty magazine. Instant (Started only). */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wielder_Action_DryFire);

	/** Reload in progress - Started with its duration, Ended when it completes or is cancelled. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Wielder_Action_Reload);
}
