// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveTestTags.h"

#if !UE_BUILD_SHIPPING
namespace EnclaveGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Equipment_Helmet, "Test.Equipment.Helmet", "PIE test: item is equipped in the helmet slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Equipment_Chest, "Test.Equipment.Chest", "PIE test: item is equipped in the chest slot.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Test_Equipment_RifleWielded, "Test.Equipment.RifleWielded", "PIE test: rifle is currently wielded.");
}
#endif // !UE_BUILD_SHIPPING
