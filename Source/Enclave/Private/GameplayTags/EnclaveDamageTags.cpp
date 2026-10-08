// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveDamageTags.h"

namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage, "Damage", "Root tag for semantic damage classification.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type, "Damage.Type", "Root for damage-kind tags.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Ballistic, "Damage.Type.Ballistic", "Conventional kinetic projectile or hitscan damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source, "Damage.Source", "Root for semantic damage-source classification.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Environment, "Damage.Source.Environment", "Damage caused by the world/environment rather than another combatant.");
}
