// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveAnimDataProviderInterface.generated.h"

/**
 * One frame's worth of animation-relevant state, as the avatar defines it.
 *
 * Returned as a single snapshot (rather than one getter per value) so that:
 * - the anim instance makes one interface call per frame on the game thread
 *   and then works from its own copy on the worker thread;
 * - adding a value means adding a field here and filling it in the provider,
 *   without changing the interface's signature;
 * - "no provider" is just a default-constructed snapshot.
 *
 * Holds raw-but-semantic inputs only. Derived values (ground speed, move
 * direction, bIsMoving) are computed by the anim instance on the worker
 * thread, so every provider doesn't have to re-implement that math.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveAnimData
{
	GENERATED_BODY()

public:
	/** Velocity in the avatar's local space (X = forward, Y = right, Z = up), cm/s. */
	UPROPERTY(BlueprintReadOnly, Category = "Anim Data")
	FVector LocalVelocity = FVector::ZeroVector;

	/**
	 * Whether the avatar is actively trying to move (input/AI acceleration),
	 * as opposed to coasting. Useful for start/stop transitions.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Anim Data")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Data")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "Anim Data")
	bool bIsCrouching = false;

	/** Aim pitch in degrees, -90 (straight down) to +90 (straight up). */
	UPROPERTY(BlueprintReadOnly, Category = "Anim Data")
	float AimPitch = 0.0f;
};

UINTERFACE(MinimalAPI)
class UEnclaveAnimDataProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract between an animated avatar and the anim instances driving its meshes.
 *
 * The avatar owns what "accelerating", "crouching", or "aim pitch" mean -
 * including any networking needed to make them correct on simulated
 * proxies - so anim instances never depend on ACharacter, the movement
 * component, or a concrete Enclave pawn class.
 *
 * Gameplay state (reloading, aiming down sights, sprinting...) deliberately
 * does NOT belong here: that arrives as Gameplay Tags through the anim
 * instance's GameplayTagPropertyMap, straight from the ASC.
 */
class ENCLAVE_API IEnclaveAnimDataProviderInterface
{
	GENERATED_BODY()

public:
	/** Called once per anim update on the game thread. Must be cheap. */
	virtual FEnclaveAnimData GetAnimData() const = 0;
};
