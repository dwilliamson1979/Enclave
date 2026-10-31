// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnclaveTargetHistoryComponent.generated.h"

class UEnclaveAvatarLifecycleComponent;
enum class EEnclaveAvatarLifecycleState : uint8;

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
};

/**
 * Stage C2: bounded, nonreplicated, authority-only capsule history per avatar.
 * No live collision is moved, and no hit-registration behavior is changed.
 *
 * An independent character lifetime has an independent history. Teleports,
 * possession and collision-policy changes fully invalidate it. Death retains
 * the bounded pre-death window for trade kills; the corpse is ineligible afterward.
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

	/** Authority-only capture notification. Consumers own their own replication. */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCapsuleFrameRecorded, const FEnclaveTargetCapsuleFrame&);
	FOnCapsuleFrameRecorded& OnCapsuleFrameRecorded() { return CapsuleFrameRecorded; }

	/** Authority only; yields a capsule at a recorded server time or fails.
	 * Historical availability does NOT grant permission to rewind: the C1
	 * timing context must independently enforce the match rewind allowance.
	 */
	bool TryGetCapsuleAtTime(double ServerTimeSeconds, FEnclaveTargetCapsuleFrame& OutFrame) const;

	/** Clear all samples at avatar identity boundaries (possession/policy/respawn). */
	/** Record authoritative death boundary without discarding pre-death snapshots. */
	void MarkDeath(double ServerDeathTimeSeconds);
	/** Exclude historical corpse without treating it as missing history. */
	bool WasDeadAtTime(double ServerTimeSeconds) const;
	/** Authoritative death boundary; -1 when no death has been recorded. */
	double GetRecordedDeathTimeSeconds() const { return DeathTimeSeconds; }
	/** Exact server-captured frame stamp. This does not prove client delivery. */
	bool HasRecordedFrameStamp(double ServerTimeSeconds) const;
	/** Maps a whole-ms wire stamp back to the exact time of the one recorded frame
	 * it was quantized from. Fails when no frame (or more than one) matches, so an
	 * echoed stamp can only ever name a frame this server actually recorded.
	 */
	bool TryResolveRecordedStampMs(uint32 StampMs, double& OutServerTimeSeconds) const;
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
	/** Owner's lifecycle events: death marks the boundary, a revive starts a fresh history. */
	void HandleOwnerLifecycleStateChanged(EEnclaveAvatarLifecycleState OldState, EEnclaveAvatarLifecycleState NewState);

	FOnCapsuleFrameRecorded CapsuleFrameRecorded;

	/** Authority only: the owner's lifecycle component, bound in BeginPlay. */
	TWeakObjectPtr<UEnclaveAvatarLifecycleComponent> OwnerLifecycle;

	/** Set from lifecycle events. Survives InvalidateHistory so a corpse is never sampled. */
	bool bOwnerDead = false;

	// The small bounded array is never replicated and contains no UObject references.
	TArray<FEnclaveTargetCapsuleFrame> Frames;
	bool bBreakBeforeNextCapture = false;
	double DeathTimeSeconds = -1.0;
	void TrimExpiredFrames(double CurrentServerTimeSeconds, double MaxRewindSeconds);
	double LastDebugSummaryTime = -1.0;
};
