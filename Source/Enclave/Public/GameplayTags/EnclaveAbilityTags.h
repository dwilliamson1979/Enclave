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

	/**
	 * Owned by the avatar's ASC for as long as UEnclaveGA_Reload is active
	 * (its ActivationOwnedTags). UEnclaveGA_Fire lists it in its
	 * ActivationBlockedTags, so a weapon can't fire mid-reload. A future
	 * "fire interrupts reload" weapon (TF2-style shotgun shell reload) would
	 * instead have its fire ability cancel abilities carrying this state.
	 */
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Reloading);
}