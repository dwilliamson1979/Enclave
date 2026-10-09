// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"

class UAbilitySystemComponent;

/**
 * Why an avatar-lifetime effect sweep is being performed.
 * Used only for diagnostics; cleanup semantics are identical for both paths.
 */
enum class EEnclaveAvatarEffectCleanupReason : uint8
{
	Death,
	AvatarReplacement
};

/**
 * Central policy for active Gameplay Effects on Enclave's persistent PlayerState ASC.
 *
 * UEnclaveGameplayEffect definitions declare one authored EffectScope property.
 * Effect.Scope.Player is the only authored scope allowed to survive the current
 * avatar; Effect.Scope.Avatar is removed. Plain/third-party Gameplay Effects may
 * participate through Effect.Scope.* asset/dynamic tags. Unscoped, conflicting,
 * or invalid persistent effects fail safe by being removed with a warning.
 */
class ENCLAVE_API FEnclaveGameplayEffectScopePolicy final
{
public:
	static int32 RemoveAvatarLifetimeEffects(UAbilitySystemComponent* AbilitySystemComponent, EEnclaveAvatarEffectCleanupReason CleanupReason);

private:
	FEnclaveGameplayEffectScopePolicy() = delete;
};
