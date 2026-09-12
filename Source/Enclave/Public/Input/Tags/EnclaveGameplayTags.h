#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Native Gameplay Tags used as stable input contracts by Enclave.
 *
 * Policy:
 * - Declare tags here when C++ routes, branches on, or otherwise depends on them.
 * - Keep content-only/detail tags in configuration unless C++ requires stable access.
 * - Tags describe semantic player intent, not physical keys or buttons.
 */
namespace EnclaveGameplayTags
{
	// ---------------------------------------------------------------------
	// Input - Root
	// ---------------------------------------------------------------------

	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input);

	// ---------------------------------------------------------------------
	// Input - Avatar / General Pawn Actions
	// ---------------------------------------------------------------------

	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Avatar);

	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Avatar_Move);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Avatar_Look);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Avatar_Jump);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Avatar_Crouch);
	ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Avatar_Sprint);

	//// ---------------------------------------------------------------------
	//// Input - Gameplay Actions
	//// ---------------------------------------------------------------------

	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action);

	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Item);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Item_Primary);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Item_Secondary);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Item_Reload);

	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Action_Interact);

	//// ---------------------------------------------------------------------
	//// Input - Drone
	//// ---------------------------------------------------------------------

	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drone);

	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drone_Move);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drone_Look);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drone_Ascend);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drone_Descend);
	//ENCLAVE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Drone_Detonate);
}