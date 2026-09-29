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
 * them) and Input.Item.* (the buttons pressed): an item translates its own
 * events into these, so a body's data never depends on how a particular
 * item's abilities or cues are named.
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

	/** Reload in progress - Started with its duration, Ended when it completes or is cancelled. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Presentation_Item_Action_Reload);
}
