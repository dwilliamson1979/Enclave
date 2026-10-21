// Copyright Lightcarrier Studios

#include "GameplayTags/EnclavePresentationTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Presentation_Item_Action, "Presentation.Item.Action", "Root tag for semantic item actions presented by an avatar.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Presentation_Item_Action_Draw, "Presentation.Item.Action.Draw", "Avatar presents drawing or equipping the item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Presentation_Item_Action_Fire, "Presentation.Item.Action.Fire", "Avatar presents firing the item once.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Presentation_Item_Action_DryFire, "Presentation.Item.Action.DryFire", "Avatar presents a dry-fire action.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Presentation_Item_Action_Focus, "Presentation.Item.Action.Focus", "Avatar presents the start and end of item Focus.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Presentation_Item_Action_Reload, "Presentation.Item.Action.Reload", "Avatar presents reloading the item.");
}
