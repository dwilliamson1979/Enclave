#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetSerialization.h"
#include "Math/RandomStream.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "Combat/EnclaveSSRGameplayClaim.h"
#include "EnclavePlayerController.generated.h"

class UEnhancedInputComponent;
class UEnclaveInputConfig;
class IEnclaveInputReceiverInterface;
class IEnclaveInputConfigProviderInterface;
class UEnclaveCombatFeedbackComponent;
class AEnclaveCharacter;

UCLASS()
class ENCLAVE_API AEnclavePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AEnclavePlayerController();

	/**
	 * Development-only lifecycle test. Recreates this controller's pawn while
	 * preserving the PlayerState-owned ASC so stale item specs/effects are easy
	 * to detect across repeated respawns.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugRespawn();

	/**
	 * Development-only inventory-removal test. Removes and destroys the currently
	 * wielded ordinary inventory item on authority, exercising Equipment fallback.
	 * The persistent default item is never removable through this command.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugRemoveWieldedItem();

	/**
	 * Development-only slot clearing test. Resolves a registered Gameplay Tag,
	 * then removes that ordinary slot's Equipment assignment on authority.
	 * Does not remove the item from Inventory or destroy it. Reserved Default
	 * and Stowed slots are intentionally excluded from this testing command.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugClearSlot(const FString& SlotTagName);

	/**
	 * C4E development rig: drive THIS locally owned Enclave pawn with normal
	 * CharacterMovement input, reversing across a short lateral lane.
	 * Enter `Enclave.Debug.SSRStrafe 1` in the target player's PIE console.
	 * Enter `Enclave.Debug.SSRStrafe 0` to stop; default duration is 30 seconds.
	 * Set `Enclave.Debug.SSRStrafeDuration 180` before starting for longer automated runs.
	 * This is not an AI/server-authoritative movement override or an SSR input.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugSSRStrafe(int32 bEnable);

	/** D1.1B: reproducible local-client shooting through normal semantic Primary/Reload
	 * input and GAS, with no separate shot RPC or server-side fire shortcut.
	 * Requires Enclave.Debug.SSRGameplayGate 1 on the client and server.
	 * Start/stop: Enclave.Debug.SSRAutoFire 1 / Enclave.Debug.SSRAutoFire 0.
	 * Optional seed CVar: Enclave.Debug.SSRAutoSeed (set BEFORE starting).
	 */
	UFUNCTION(Exec)
	void EnclaveDebugSSRAutoFire(int32 bEnable);

	/** D1.1C test protection for locally or remotely owned stationary/crouching targets.
	 * Requires SSRGameplayGate on the server. Remote client requests use an owner-only server RPC.
	 * Explicitly disable after the test; never replicated.
	 * Use Enclave.Debug.SSRProtect 1 / 0.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugSSRProtect(int32 bEnable);

	/** Development-only crouch input on the locally possessed test target.
	 * Uses normal CharacterMovement crouch prediction/replication, including
	 * autonomous clients. Local capsule verification is NOT server verification.
	 * Stable crouch may be combined with the local strafe/reversal rig; transitions
	 * while strafing are blocked to preserve scenario identity.
	 * Run Enclave.Debug.SSRCrouch 1 / 0 on the TARGET's own PIE console.
	 * Requires Enclave.Debug.SSRGameplayGate 1 to start; protection is separate.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugSSRCrouch(int32 bEnable);

	/** Opt-in, non-gameplay per-shot camera-vs-authoritative-ray diagnostic. */
	static bool IsShotAlignmentDebugEnabled();

	/** Optional, no additional RPC: classify a mesh-anchored proxy capsule.
	 * Clear misses are restricted to 10–40 cm outside the reference capsule.
	 * Strictly DEVELOPMENT diagnostics; never hit authority.
	 */
	static bool IsSSRGameplayGateEnabled();
	bool DebugBuildSSRGameplayClaim(int32 PredictionKey, const FVector& ShotOrigin,
		const FVector& ShotDirection, FEnclaveSSRGameplayClaim& OutClaim) const;


	/** Called only by a locally controlled GA_Fire activation (after CommitAbility). */
	void DebugCaptureLocalShot(int32 PredictionKey);

	/** Opt-in Stage C4F clock-only probe. Not part of validated shot intent.
	 * The server compares process times only in same-process PIE.
	 */
	void DebugProbeSSRShotClock(int32 PredictionKey, double ClientShotServerTimeSeconds);

	static bool IsSSRClockDebugEnabled();

	/** Bounded echo feasibility probe: client samples a simulated proxy near its view ray. */
	void DebugProbeSSREchoedSnapshot(int32 PredictionKey);


	/** Observe GameState time-estimate corrections and PIE frame phases without changing the sync algorithm. */
	void DebugTickSSRClockSync();

	/** Stage C1: sample a locally simulated patrol's visual proxy anchor at shot capture.
	 * Debug-only, no effect on hit registration. Sends an unreliable telemetry RPC.
	 */
	void DebugLogSSRProxySample(int32 PredictionKey, double ClientShotServerTimeSeconds);

	/** Brackets the synchronous server-side fireable hit query; no gameplay inputs change. */
	void DebugBeginAuthoritativeShot(int32 PredictionKey);
	void DebugEndAuthoritativeShot();

	/** Called with the REAL server hitscan query by HitRegistrationSubsystem. */
	void DebugRecordAuthoritativeTrace(const FVector& Start, const FVector& End, bool bHit, const FVector& ImpactPoint);

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;
	virtual void OnUnPossess() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupInputComponent() override;

	/**
	 * Determines whether a semantic input event should be forwarded to gameplay.
	 *
	 * Override this for controller-level gating such as menus, possession
	 * transitions, global input suppression, or other player-wide policies.
	 */
	virtual bool CanProcessInput(const FEnclaveInputEvent& InputEvent) const;

private:

	/** Optional debug-only telemetry; no gameplay decision may consume this RPC.
	 * Its independent arrival time must not be confused with Fire target-data receipt.
	 */
	/** Untrusted diagnostics only: independent from the GAS shot-intent RPC. */
	UFUNCTION(Server, Unreliable)
	void ServerEnclaveDebugShotClockProbe(int32 PredictionKey, uint32 ClientProcessId,
		double ClientCaptureProcessSeconds, double ClientCaptureWorldSeconds,
		double ClientEstimatedServerSeconds);

	UFUNCTION(Server, Unreliable)
	void ServerEnclaveDebugPatrolVisualSample(int32 PredictionKey, AEnclaveCharacter* PatrolTarget,
		FVector_NetQuantize10 ClientVisualProxyAnchor, double ClientShotServerTimeSeconds,
		uint32 ClientProcessId, double ClientVisualCaptureProcessSeconds,
		double ClientVisualSampleWorldSeconds, double ClientVisualSampleServerEstimateSeconds);

	/** Untrusted opt-in telemetry, NEVER gameplay hit authority. */
	UFUNCTION(Server, Unreliable)
	void ServerEnclaveDebugEchoedSnapshot(int32 PredictionKey, AEnclaveCharacter* Target,
		double EchoedMovementStamp, float ElapsedSinceUpdateSeconds,
		FVector_NetQuantize10 ReplicatedLocation, FVector_NetQuantize10 ReplicatedVelocity,
		FVector_NetQuantize10 CapsuleAtReceive, FVector_NetQuantize10 ClientCapsuleAtShot,
		FVector_NetQuantize10 ClientMeshAnchor);

	/** Debug-only request from a remotely owned target; validates possession and gate on authority. */
	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugSSRProtect(bool bEnable);

	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugRespawn();

	void PerformDebugRespawn();

	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugRemoveWieldedItem();

	void PerformDebugRemoveWieldedItem();

	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugClearSlot(FGameplayTag SlotTag);

	void PerformDebugClearSlot(FGameplayTag SlotTag);

	UFUNCTION(Client, Reliable)
	void ClientEnclaveDebugClearSlotResult(bool bSucceeded, const FString& ResultMessage);

	/** Debug-only telemetry. Sent ONLY to the owning controller, not replicated gameplay data. */
	UFUNCTION(Client, Reliable)
	void ClientEnclaveDebugShotAlignment(int32 PredictionKey, FVector ServerStart, FVector ServerEnd, bool bHit, FVector ImpactPoint);

	struct FDebugShotRay
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		double RecordedTime = 0.0;
	};

	struct FDebugServerShot : FDebugShotRay
	{
		FVector ImpactPoint = FVector::ZeroVector;
		bool bHit = false;
	};

	void DebugCompareShotIfReady(int32 PredictionKey);
	void DebugPruneShotHistory();

	// Diagnostic-only transient state. No replicated properties, no fire behavior changes.
	int32 ActiveServerShotKey = INDEX_NONE;
	TMap<int32, FDebugShotRay> PendingLocalShots;
	TMap<int32, FDebugServerShot> PendingServerShots;

	// C4 diagnostic RPCs must not queue unbounded delayed history comparisons.
	int32 PendingSSRVisualTimingMatches = 0;

#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
	// C4E test rig state is local to the owning PlayerController; never replicated.
	void StopDebugSSRStrafe(const TCHAR* Reason);
	FVector DebugSSRStrafeOrigin = FVector::ZeroVector;
	FVector DebugSSRStrafeRight = FVector::ZeroVector;
	float DebugSSRStrafeElapsedSeconds = 0.0f;
	float DebugSSRStrafePauseSeconds = 0.0f;
	float DebugSSRStrafeNextStatusSeconds = 0.0f;
	int32 DebugSSRStrafeDirectionSign = 1;
	bool bDebugSSRStrafeEnabled = false;
	TWeakObjectPtr<AEnclaveCharacter> DebugSSRStrafeProtectedPawn;
	TWeakObjectPtr<AEnclaveCharacter> DebugSSRManualProtectedPawn;

	// D1.1C.1: authority-local crouch rig; never replicated.
	void StopDebugSSRCrouch(const TCHAR* Reason);
	void TickDebugSSRCrouch(float DeltaSeconds);
	TWeakObjectPtr<AEnclaveCharacter> DebugSSRCrouchPawn;
	float DebugSSRCrouchWaitSeconds = 0.0f;
	bool bDebugSSRCrouchDesired = false;
	bool bDebugSSRCrouchPending = false;
	bool bDebugSSRStrafeQueuedForCrouch = false;

	// D1.1B automated shot sampling; never replicated or compiled for Test/Shipping.
	void StopDebugSSRAutoFire(const TCHAR* Reason);
	void TickDebugSSRAutoFire(float DeltaSeconds);
	bool ChooseDebugSSRAutoTarget();
	TWeakObjectPtr<AEnclaveCharacter> DebugSSRAutoTarget;
	FRandomStream DebugSSRAutoRandom;
	TWeakObjectPtr<APawn> DebugSSRAutoOriginalPawn;
	float DebugSSRAutoElapsedSeconds = 0.0f;
	float DebugSSRAutoUntilNextShotSeconds = 0.0f;
	float DebugSSRAutoReloadDelaySeconds = 0.0f;
	int32 DebugSSRAutoAttemptCount = 0;
	int32 DebugSSRAutoReloadCount = 0;
	int32 DebugSSRAutoConsecutiveReloadFailures = 0;
	bool bDebugSSRAutoHasObservedMovingTarget = false;
	bool bDebugSSRAutoEnabled = false;

	// C4F.1: private, nonreplicated, opt-in clock-observation state.
	// Compare adjacent frames to identify server-time estimate offset steps;
	// compare adjacent process/world deltas to expose PIE frame stalls.
	bool bDebugSSRClockSyncHasSample = false;
	double DebugSSRClockSyncLastProcessSeconds = 0.0;
	double DebugSSRClockSyncLastWorldSeconds = 0.0;
	double DebugSSRClockSyncLastEstimatedSeconds = 0.0;
	double DebugSSRClockSyncLastOffsetSeconds = 0.0;
	double DebugSSRClockSyncLastReportProcessSeconds = 0.0;
	double DebugSSRClockSyncLastReportWorldSeconds = 0.0;
	double DebugSSRClockSyncLastReportEstimatedSeconds = 0.0;
	double DebugSSRClockSyncLastReportOffsetSeconds = 0.0;
#endif

	void HandleInput(const FInputActionValue& Value, ETriggerEvent TriggerEvent, FGameplayTag InputTag, bool bRoutesReleaseEvents);

	void RefreshPossessedPawnInputState();

	void ActivateInputConfig(const UEnclaveInputConfig* InputConfig);
	void DeactivateInputConfig(const UEnclaveInputConfig* InputConfig);

	void AddInputConfigMappingContexts(const UEnclaveInputConfig* InputConfig);
	void RemoveInputConfigMappingContexts(const UEnclaveInputConfig* InputConfig);

	void BindInputConfigActions(UEnhancedInputComponent& EnhancedInput, const UEnclaveInputConfig* InputConfig);
	void ClearInputBindings();

	IEnclaveInputReceiverInterface* CachedInputReceiver = nullptr;
	IEnclaveInputConfigProviderInterface* CachedInputConfigProvider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UEnclaveInputConfig> ActiveAvatarInputConfig = nullptr;

	TArray<uint32> ActiveInputBindingHandles;

	/**
	 * Hosts local-player combat feedback (hit marker today). Implements
	 * IEnclaveHitFeedbackInterface - callers (e.g. UEnclaveGA_Fire) find it
	 * via FindComponentByInterface rather than depending on this class, the
	 * same idiom UEnclaveEquipmentComponent uses to find its item source.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveCombatFeedbackComponent> CombatFeedbackComp = nullptr;
};