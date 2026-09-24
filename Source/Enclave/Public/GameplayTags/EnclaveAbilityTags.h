// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace EnclaveGameplayTags
{
	/** Set-by-caller magnitude tag used on damage Gameplay Effects (e.g. GE_Fire_Damage). */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Rifle_Fire);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Weapon_Pistol_Fire);
}