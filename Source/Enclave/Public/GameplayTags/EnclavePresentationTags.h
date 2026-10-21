// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Item presentation action tags - semantic actions the avatar may visibly
 * present while using an item. Carried by FEnclaveItemPresentationActionEvent
 * and used as keys in the per-item-type montage table on UEnclaveItemPresentationDefinition.
 *
 * Deliberately separate from GameplayCue.* (the gameplay events behind
 * them) and Input.Item.* (the buttons pressed): the Gameplay Cue presentation
 * boundary translates established gameplay events into these semantic actions,
 * so a body's data never depends on how a particular ability or cue is named.
 */
namespace EnclaveGameplayTags
{
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action);

	/** Item draw/equip presentation action. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action_Draw);

	/** One shot fired. Instant (Started only). */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action_Fire);

	/** Trigger pulled on an empty magazine. Instant (Started only). */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action_DryFire);

	/** Focus in progress - Started/Ended, including ability cancellation. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action_Focus);

	/** Reload in progress - Started with its duration, Ended when it completes or is cancelled. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action_Reload);
}
