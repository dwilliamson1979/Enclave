// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveGameplayCueTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Action, "GameplayCue.Item.Action", "Root tag for semantic item-action Gameplay Cues.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Action_Fire, "GameplayCue.Item.Action.Fire", "One-shot cue emitted when an item successfully fires.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Action_Reload, "GameplayCue.Item.Action.Reload", "Duration cue emitted while an item reload action is active.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(GameplayCue_Item_Action_DryFire, "GameplayCue.Item.Action.DryFire", "One-shot cue emitted when an item attempts to fire without available ammo.");
}