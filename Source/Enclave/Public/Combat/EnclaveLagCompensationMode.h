// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "EnclaveLagCompensationMode.generated.h"

/** Temporal policy; independent of Capsule vs SkeletalHitboxes collision geometry.
 * ServerSideRewind is reserved and rejected at runtime until SSR is implemented.
 */
UENUM(BlueprintType)
enum class EEnclaveLagCompensationMode : uint8
{
	Disabled UMETA(DisplayName = "Disabled (Present-Time)"),
	ServerSideRewind UMETA(DisplayName = "Server-Side Rewind (Not Implemented)")
};
