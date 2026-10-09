// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "EnclaveGameplayEffect.generated.h"

/**
 * Common base for Enclave-authored Gameplay Effects.
 *
 * Persistent effects must declare which lifetime owns them. Avatar is the safe
 * default because the player ASC lives on PlayerState and can outlive a Pawn.
 * Effects intentionally allowed to survive avatar replacement must explicitly
 * opt into Effect.Scope.Player.
 *
 * Instant effects never enter the active-effect container, so their scope does
 * not affect cleanup even though they inherit the same authored default.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API UEnclaveGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UEnclaveGameplayEffect();

	/** Authored lifetime scope used by FEnclaveGameplayEffectScopePolicy. */
	FGameplayTag GetEffectScope() const { return EffectScope; }

protected:
	/**
	 * Lifetime owner for this effect. The picker is intentionally restricted to
	 * Effect.Scope.* tags. Effect.Scope.Avatar is the safe default.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enclave|Lifecycle", meta = (Categories = "Effect.Scope"))
	FGameplayTag EffectScope;
};
