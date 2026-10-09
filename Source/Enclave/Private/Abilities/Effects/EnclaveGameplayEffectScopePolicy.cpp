// Copyright Lightcarrier Studios

#include "Abilities/Effects/EnclaveGameplayEffectScopePolicy.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Actor.h"

#include "Abilities/Effects/EnclaveGameplayEffect.h"
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

	bool ShouldPreserveEnclaveGameplayEffect(const UEnclaveGameplayEffect* GameplayEffectDefinition, const TCHAR* CleanupReasonString)
	{
		check(GameplayEffectDefinition);

		const FGameplayTag EffectScope = GameplayEffectDefinition->GetEffectScope();

		if (EffectScope.MatchesTagExact(EnclaveGameplayTags::Effect_Scope_Player))
		{
			return true;
		}

		if (EffectScope.MatchesTagExact(EnclaveGameplayTags::Effect_Scope_Avatar))
		{
			return false;
		}

		UE_LOG(
			LogEnclaveAbilities,
			Warning,
			TEXT("Persistent Enclave Gameplay Effect %s has invalid EffectScope %s during %s. Removing it as avatar-lifetime state. EffectScope must be Effect.Scope.Avatar or Effect.Scope.Player."),
			*GetNameSafe(GameplayEffectDefinition),
			*EffectScope.ToString(),
			CleanupReasonString);

		return false;
	}

	bool ShouldPreserveExternalGameplayEffect(const FActiveGameplayEffect& ActiveEffect, const TCHAR* CleanupReasonString)
	{
		FGameplayTagContainer EffectAssetTags;
		ActiveEffect.Spec.GetAllAssetTags(EffectAssetTags);

		const bool bAvatarScoped = EffectAssetTags.HasTagExact(EnclaveGameplayTags::Effect_Scope_Avatar);
		const bool bPlayerScoped = EffectAssetTags.HasTagExact(EnclaveGameplayTags::Effect_Scope_Player);

		if (bPlayerScoped && !bAvatarScoped)
		{
			return true;
		}

		const UGameplayEffect* GameplayEffectDefinition = ActiveEffect.Spec.Def;
		const FString EffectName = GetNameSafe(GameplayEffectDefinition);

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
				TEXT("Persistent Gameplay Effect %s has no Effect.Scope classification during %s. Removing it as avatar-lifetime state. Prefer UEnclaveGameplayEffect for authored effects, or tag intentional external persistent effects with Effect.Scope.Player."),
				*EffectName,
				CleanupReasonString);
		}

		return false;
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

	// Instant effects execute immediately and never enter the active-effect
	// container. Every effect here is therefore Duration or Infinite and needs
	// a lifetime decision before it may outlive the current avatar.
	const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles = AbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery());
	const TCHAR* CleanupReasonString = GetCleanupReasonString(CleanupReason);
	int32 RemovedCount = 0;

	for (const FActiveGameplayEffectHandle& ActiveEffectHandle : ActiveEffectHandles)
	{
		const FActiveGameplayEffect* ActiveEffect = AbilitySystemComponent->GetActiveGameplayEffect(ActiveEffectHandle);
		if (!ActiveEffect || !IsValid(ActiveEffect->Spec.Def))
		{
			continue;
		}

		bool bPreserve = false;

		if (const UEnclaveGameplayEffect* EnclaveGameplayEffect = Cast<UEnclaveGameplayEffect>(ActiveEffect->Spec.Def))
		{
			// Enclave-authored effects use the strongly authored single scope
			// property as their source of truth. No tag/property synchronization is
			// required and mutually-conflicting authored scopes are impossible.
			bPreserve = ShouldPreserveEnclaveGameplayEffect(EnclaveGameplayEffect, CleanupReasonString);
		}
		else
		{
			// Plain/third-party Gameplay Effects still participate through asset or
			// dynamic Effect.Scope.* tags. This also covers dynamically-scoped
			// cooldown specs created by Enclave item abilities.
			bPreserve = ShouldPreserveExternalGameplayEffect(*ActiveEffect, CleanupReasonString);
		}

		if (bPreserve)
		{
			continue;
		}

		if (AbilitySystemComponent->RemoveActiveGameplayEffect(ActiveEffectHandle, -1))
		{
			++RemovedCount;
		}
	}

	return RemovedCount;
}
