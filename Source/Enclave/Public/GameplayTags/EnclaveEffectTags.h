// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace EnclaveGameplayTags
{
	/** Root namespace describing how long an active Gameplay Effect may outlive an avatar. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Scope);

	/** Effect belongs to one Pawn/avatar lifetime and must not carry into a replacement avatar. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Scope_Avatar);

	/** Effect intentionally belongs to the persistent player/ASC rather than a particular avatar. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Scope_Player);
}
