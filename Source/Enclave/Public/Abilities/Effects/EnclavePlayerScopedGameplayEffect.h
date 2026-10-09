// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EnclavePlayerScopedGameplayEffect.generated.h"

/**
 * Base Gameplay Effect for state that intentionally belongs to the persistent
 * player/ASC and may survive Pawn replacement.
 *
 * Use this base deliberately. Most temporary combat state should instead use
 * UEnclaveAvatarScopedGameplayEffect (or receive Effect.Scope.Avatar dynamically).
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclavePlayerScopedGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclavePlayerScopedGameplayEffect();
};
