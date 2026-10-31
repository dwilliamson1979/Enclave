// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Debug/EnclaveDebug.h"
#include "EnclaveHitTargetComponent.generated.h"

class UEnclaveTargetHistoryComponent;
struct FEnclaveTargetCapsuleFrame;

/** Server-authored stamp of a single authoritative history frame, echoed back by
 * shooters as SSR timing evidence. It does not replace FRepMovement.
 * Wire cost: a packed whole-millisecond stamp (3-4 bytes), identical in every build
 * so that bandwidth measured in Development matches Shipping.
 */
USTRUCT()
struct FEnclaveSSRServerCapsuleObservation
{
	GENERATED_BODY()

	/** GameState server world time of the recorded frame, rounded to whole ms. */
	UPROPERTY() uint32 ServerTimeMs = 0;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FEnclaveSSRServerCapsuleObservation> : TStructOpsTypeTraitsBase2<FEnclaveSSRServerCapsuleObservation>
{
	enum { WithNetSerializer = true };
};

/** Last received authoritative observation; local arrival time is not a network clock. */
struct FEnclaveSSRReceivedProxyUpdate
{
	double ServerStamp = 0.0;
	FVector ReplicatedLocation = FVector::ZeroVector;
	FVector ReplicatedVelocity = FVector::ZeroVector;
	FVector CapsuleAtReceive = FVector::ZeroVector;
	double ClientReceiveWorldSeconds = -1.0;
	uint32 ReceiveCount = 0;
};

/**
 * This character as a target of hit registration: the per-character
 * counterpart to the match-wide UEnclaveCombatPolicyComponent.
 *
 * - Server: applies the policy's hit detection mode to the owner's capsule and
 *   third-person mesh, and re-applies it when the policy changes.
 * - Server: publishes a replicated stamp each time the history component
 *   records a frame in which the capsule moved (SSR timing evidence).
 * - Simulated proxies: record the last stamp received, which the shooter's
 *   client echoes back with its shot (Hybrid and EchoedStamp timing).
 *
 * The capsule history itself stays in UEnclaveTargetHistoryComponent. The owner
 * must be an ACharacter, which provides the capsule and the third-person mesh.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveHitTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnclaveHitTargetComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	/** Simulated proxies: the last authoritative stamp received for this character. */
	const FEnclaveSSRReceivedProxyUpdate& GetLastReceivedSSRObservation() const { return LastReceivedSSRObservation; }

#if ENCLAVE_INCLUDE_DEBUG_CODE
	/** Simulated proxies: server stamps received in the last second, oldest first (abuse tests). */
	const TArray<double>& DebugGetReceivedSSRStamps() const { return DebugReceivedSSRStamps; }
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool HasOwnerAuthority() const;

	/** Responds to authoritative match hit-detection policy changes. */
	void HandleWeaponHitDetectionModeChanged(EEnclaveWeaponHitDetectionMode NewMode);
	/** Applies the selected policy to the owner's collision components. Authority only. */
	void ApplyWeaponHitDetectionCollisionPolicy(EEnclaveWeaponHitDetectionMode Mode);

	/** Called when the history component records an authoritative sample. */
	void HandleCapsuleFrameRecorded(const FEnclaveTargetCapsuleFrame& Frame);

	// A single replicated struct carries one coherent authoritative history sample.
	UPROPERTY(ReplicatedUsing = OnRep_SSRServerCapsuleObservation, Transient)
	FEnclaveSSRServerCapsuleObservation SSRServerCapsuleObservation;

	UFUNCTION()
	void OnRep_SSRServerCapsuleObservation();

	FEnclaveSSRReceivedProxyUpdate LastReceivedSSRObservation;

	/** Authority only: the owner's history component, bound in BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<UEnclaveTargetHistoryComponent> TargetHistory = nullptr;

	// Authority only: the capsule last published as a stamp. A character that has not
	// moved keeps its old stamp, so idle characters do not replicate every tick.
	FVector LastStampedCenter = FVector::ZeroVector;
	float LastStampedRadiusCm = 0.0f;
	float LastStampedHalfHeightCm = 0.0f;

	// Preserve the configured animation tick behavior when temporarily forcing skeletal hitboxes.
	EVisibilityBasedAnimTickOption SavedThirdPersonAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	bool bSavedThirdPersonAnimTickOption = false;

#if ENCLAVE_INCLUDE_DEBUG_CODE
	TArray<double> DebugReceivedSSRStamps;
#endif
};
