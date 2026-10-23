// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnclaveTargetHistoryComponent.generated.h"

/**
 * One server-observed capsule snapshot. Times use the same authoritative
 * GameState server-world-time domain as FEnclaveShotTimingContext.
 * This is NOT a skeletal Physics Asset pose or a client-rendered frame.
 */
struct ENCLAVE_API FEnclaveTargetCapsuleFrame
{
	double ServerTimeSeconds = 0.0;
	FVector Center = FVector::ZeroVector;
	FQuat Rotation = FQuat::Identity;
	float RadiusCm = 0.0f;
	float HalfHeightCm = 0.0f;
};

/**
 * Stage C2: bounded, nonreplicated, authority-only capsule history per avatar.
 * No live collision is moved, and no hit-registration behavior is changed.
 *
 * An independent character lifetime has an independent history. Teleports,
 * suspect jumps, time gaps, collision-shape changes, and death reset it.
 * Interpolation is allowed only within a continuous recorded segment;
 * requests outside available history fail instead of extrapolating.
 *
 * Stage C3 can query capsule history through TryGetCapsuleAtTime(). Skeletal
 * hitbox rewind requires separately recorded bone/body shapes, not this data.
 */
UCLASS(ClassGroup = (Enclave))
class ENCLAVE_API UEnclaveTargetHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnclaveTargetHistoryComponent();

	/** Authority only; yields a capsule at a recorded server time or fails.
	 * Historical availability does NOT grant permission to rewind: the C1
	 * timing context must independently enforce the match rewind allowance.
	 */
	bool TryGetCapsuleAtTime(double ServerTimeSeconds, FEnclaveTargetCapsuleFrame& OutFrame) const;

	/** Clear all samples at any avatar/history identity boundary. */
	void InvalidateHistory();

	/** Read-only diagnostics; does not expose mutable history or client data. */
	int32 GetHistorySampleCount() const { return Frames.Num(); }
	bool GetHistoryTimeBounds(double& OutOldestTime, double& OutNewestTime) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// The small bounded array is never replicated and contains no UObject references.
	TArray<FEnclaveTargetCapsuleFrame> Frames;
	double LastDebugSummaryTime = -1.0;
};
