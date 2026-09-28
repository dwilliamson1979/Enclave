// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveWielderTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wielder_Action, "Wielder.Action", "Root tag for actions a wielder visibly performs with its wielded item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wielder_Action_Draw, "Wielder.Action.Draw", "Wielder takes the item in hand.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wielder_Action_Fire, "Wielder.Action.Fire", "Wielder fires the item once.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wielder_Action_DryFire, "Wielder.Action.DryFire", "Wielder pulls the trigger on an empty item.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Wielder_Action_Reload, "Wielder.Action.Reload", "Wielder reloads the item.");
}
