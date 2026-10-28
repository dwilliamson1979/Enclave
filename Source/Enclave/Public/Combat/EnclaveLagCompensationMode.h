// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "EnclaveLagCompensationMode.generated.h"

/** Temporal policy; independent of Capsule vs SkeletalHitboxes collision geometry.
 * ServerSideRewind applies to Capsule mode only; SkeletalHitboxes stays present-time
 * until skeletal history exists.
 */
UENUM(BlueprintType)
enum class EEnclaveLagCompensationMode : uint8
{
	Disabled UMETA(DisplayName = "Disabled (Present-Time)"),
	ServerSideRewind UMETA(DisplayName = "Server-Side Rewind (Capsule)")
};

/** How the server picks the historical time for a validated remote shot (D11D-3D).
 * Server-only match policy; clients never select it.
 */
UENUM(BlueprintType)
enum class EEnclaveSSRTimingSource : uint8
{
	/** Every target at serverReceipt - (RTT + AnchorOffset). No client timing evidence. */
	ServerAnchor UMETA(DisplayName = "Server Anchor"),
	/** Every target at the client-echoed server stamp; no verifiable stamp means unavailable. */
	EchoedStamp UMETA(DisplayName = "Echoed Stamp"),
	/** Client's locally hit target at its verified echoed stamp; everyone else at the server anchor. */
	Hybrid UMETA(DisplayName = "Hybrid (Echoed Stamp + Server Anchor)")
};
