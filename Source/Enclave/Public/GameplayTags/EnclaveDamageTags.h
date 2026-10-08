// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace EnclaveGameplayTags
{
	/** Root semantic namespace for damage classification. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);

	/** Root for damage-kind tags such as Ballistic, Fire, EMP, etc. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type);

	/** Conventional projectile/hitscan kinetic damage. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Type_Ballistic);

	/** Root for semantic damage-source classification. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source);

	/** Damage caused by the world/environment rather than another combatant. */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Environment);
}
