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
 * Active effects explicitly tagged Effect.Scope.Player are the only effects allowed
 * to survive the current avatar. Effect.Scope.Avatar effects are removed normally.
 * Unscoped or conflicting persistent effects are also removed and diagnosed so an
 * authoring omission fails safe instead of leaking state into another Pawn.
 */
class ENCLAVE_API FEnclaveGameplayEffectScopePolicy final
{
public:
	static int32 RemoveAvatarLifetimeEffects(UAbilitySystemComponent* AbilitySystemComponent, EEnclaveAvatarEffectCleanupReason CleanupReason);

private:
	FEnclaveGameplayEffectScopePolicy() = delete;
};
