// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveEffectTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Scope, "Effect.Scope", "Root namespace for Gameplay Effect lifetime scope.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Scope_Avatar, "Effect.Scope.Avatar", "Active effect belongs to one avatar lifetime and is removed before a replacement avatar initializes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Scope_Player, "Effect.Scope.Player", "Active effect intentionally belongs to the persistent player/ASC and may survive avatar replacement.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Cleanup_Avatar_InProgress, "Effect.Cleanup.Avatar.InProgress", "Transient internal guard while avatar-scoped Gameplay Effects are removed from an ASC.");
}
