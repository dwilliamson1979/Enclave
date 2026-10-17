// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// These tags are for PIE/development validation only.
// IMPORTANT: Assets referring to them must not be cooked into Shipping builds.
#if !UE_BUILD_SHIPPING
namespace EnclaveGameplayTags
{
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Test_Equipment_Helmet);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Test_Equipment_Chest);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Test_Equipment_RifleWielded);
}
#endif // !UE_BUILD_SHIPPING
