// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclaveGameplayEffectScopePolicy.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"

#include "Debug/EnclaveDebug.h"
#include "GameplayTags/EnclaveEffectTags.h"

namespace
{
	const TCHAR* GetCleanupReasonString(EEnclaveAvatarEffectCleanupReason CleanupReason)
	{
		switch (CleanupReason)
		{
		case EEnclaveAvatarEffectCleanupReason::Death:
			return TEXT("death");
		case EEnclaveAvatarEffectCleanupReason::AvatarReplacement:
			return TEXT("avatar replacement");
		default:
			return TEXT("avatar cleanup");
		}
	}
}

int32 FEnclaveGameplayEffectScopePolicy::RemoveAvatarLifetimeEffects(UAbilitySystemComponent* AbilitySystemComponent, EEnclaveAvatarEffectCleanupReason CleanupReason)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return 0;
	}

	const AActor* OwnerActor = AbilitySystemComponent->GetOwnerActor();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return 0;
	}

	// Instant effects are executed immediately and never enter the active-effect
	// container. An empty query therefore gives us exactly the Duration/Infinite
	// effects whose lifetime must be classified.
	const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles = AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery());
	int32 RemovedCount = 0;

	for (const FActiveGameplayEffectHandle& ActiveEffectHandle : ActiveEffectHandles)
	{
		const FActiveGameplayEffect* ActiveEffect = AbilitySystemComponent->GetActiveGameplayEffect(ActiveEffectHandle);
		if (!ActiveEffect)
		{
			continue;
		}

		FGameplayTagContainer EffectAssetTags;
		ActiveEffect->Spec.GetAllAssetTags(EffectAssetTags);

		const bool bAvatarScoped = EffectAssetTags.HasTagExact(EnclaveGameplayTags::Effect_Scope_Avatar);
		const bool bPlayerScoped = EffectAssetTags.HasTagExact(EnclaveGameplayTags::Effect_Scope_Player);

		// Player scope is the explicit opt-in to survive the body. Conflicting
		// classification is treated as unsafe and removed rather than guessed.
		if (bPlayerScoped && !bAvatarScoped)
		{
			continue;
		}

		const UGameplayEffect* GameplayEffectDefinition = ActiveEffect->Spec.Def;
		const FString EffectName = GetNameSafe(GameplayEffectDefinition);
		const TCHAR* CleanupReasonString = GetCleanupReasonString(CleanupReason);

		if (bAvatarScoped && bPlayerScoped)
		{
			UE_LOG(
				LogEnclaveAbilities,
				Warning,
				TEXT("Gameplay Effect %s has both Effect.Scope.Avatar and Effect.Scope.Player during %s. Removing it as avatar-lifetime state."),
				*EffectName,
				CleanupReasonString);
		}
		else if (!bAvatarScoped)
		{
			UE_LOG(
				LogEnclaveAbilities,
				Warning,
				TEXT("Persistent Gameplay Effect %s has no Effect.Scope classification during %s. Removing it as avatar-lifetime state. Tag intentional persistent effects with Effect.Scope.Player."),
				*EffectName,
				CleanupReasonString);
		}

		if (AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveEffectHandle, -1))
		{
			++RemovedCount;
		}
	}

	return RemovedCount;
}
