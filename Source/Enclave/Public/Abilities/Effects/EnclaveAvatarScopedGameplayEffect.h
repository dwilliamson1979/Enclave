// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "EnclaveAvatarScopedGameplayEffect.generated.h"

/**
 * Base Gameplay Effect for state that belongs to one avatar/Pawn lifetime.
 *
 * The effect receives the Effect.Scope.Avatar asset tag. Because Enclave's
 * player ASC lives on PlayerState and survives Pawn replacement, the avatar
 * initialization boundary removes active effects with this tag before any
 * new-avatar ASC consumer initializes.
 *
 * Duration/infinite effects such as death, equip windows, burns, slows and
 * temporary avatar buffs should derive from this class when they must not
 * carry into a replacement Pawn. Instant effects do not need a lifetime scope.
 *
 * Enclave treats unscoped active effects as avatar-lifetime state during cleanup
 * (with a warning), so only Effect.Scope.Player is allowed to persist by default.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveAvatarScopedGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclaveAvatarScopedGameplayEffect();
};
