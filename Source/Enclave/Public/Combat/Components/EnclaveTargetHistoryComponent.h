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
	// True when this sample begins a new continuous movement/time segment.
	// A query may return either endpoint but MUST NOT interpolate across the boundary.
	bool bBreakBefore = false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// C4B diagnostic-only process-monotonic capture; NEVER a network rewind clock.
	FVector DebugVelocity = FVector::ZeroVector;
	double DebugCaptureProcessSeconds = 0.0;
	double DebugCaptureWorldSeconds = 0.0;
	uint64 DebugCaptureFrame = 0;
#endif
};

/**
 * Diagnostic inverse position-to-time lookup against recorded server capsule
 * centers. A positive SignedOffsetSeconds means the client's sampled visual
 * anchor best matches server target movement AFTER its supplied shot time.
 * This is NOT trusted gameplay timing and NOT a measured presentation delay.
 */
struct ENCLAVE_API FEnclaveVisualHistoryTimeMatch
{
	double MatchedServerTimeSeconds = 0.0;
	double SignedOffsetSeconds = 0.0;
	double ResidualCm = 0.0;
	double MatchedSpeedCmPerSecond = 0.0;
	int32 EvaluatedSegments = 0;
	bool bAmbiguous = false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// Interpolated within the same authoritative history segment as the fit.
	// Comparability to the client's monotonic clock REQUIRES the same process ID.
	double DebugMatchedProcessSeconds = 0.0;
	double DebugMatchedWorldSeconds = 0.0;
	uint64 DebugOlderFrame = 0;
	uint64 DebugNewerFrame = 0;
#endif
};

/**
 * Stage C2: bounded, nonreplicated, authority-only capsule history per avatar.
 * No live collision is moved, and no hit-registration behavior is changed.
 *
 * An independent character lifetime has an independent history. Teleports,
 * death, possession, and collision-policy changes fully invalidate it.
 * Teleports, suspect movement jumps, and capture gaps mark segment breaks.
 * Crouch/uncrouch preserve history: each frame records its own capsule size.
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

	/** Debug-only trajectory inversion for the separately reported patrol visual anchor.
	 * Only moves along recorded continuous server segments; never extrapolates.
	 * The result must never influence damage, firing, or the rewind policy.
	 */
	bool TryMatchVisualAnchorToHistory(const FVector& VisualAnchor, double ClientShotTimeSeconds,
		double LookbackSeconds, double LookaheadSeconds, FEnclaveVisualHistoryTimeMatch& OutMatch) const;

	/** Clear all samples at avatar identity boundaries (death/possession/policy). */
	void InvalidateHistory();

	/** Preserve preceding history, but forbid interpolation into the next sample.
	 * Call for an explicit teleport or scripted discontinuous reposition on authority.
	 * Unknown large movement jumps and frame gaps are also detected automatically.
	 */
	void MarkHistoryDiscontinuity();

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
	bool bBreakBeforeNextCapture = false;
	double LastDebugSummaryTime = -1.0;
};
