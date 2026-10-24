#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetSerialization.h"
#include "Input/Core/EnclaveInputTypes.h"
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
	 * Enter `EnclaveDebugSSRStrafe 1` in the target player's PIE console.
	 * Enter `EnclaveDebugSSRStrafe 0` to stop; also stops after 30 seconds.
	 * This is not an AI/server-authoritative movement override or an SSR input.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugSSRStrafe(int32 bEnable);

	/** Opt-in, non-gameplay per-shot camera-vs-authoritative-ray diagnostic. */
	static bool IsShotAlignmentDebugEnabled();

	/** Called only by a locally controlled GA_Fire activation (after CommitAbility). */
	void DebugCaptureLocalShot(int32 PredictionKey);

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
	UFUNCTION(Server, Unreliable)
	void ServerEnclaveDebugPatrolVisualSample(int32 PredictionKey, AEnclaveCharacter* PatrolTarget,
		FVector_NetQuantize10 ClientVisualProxyAnchor, double ClientShotServerTimeSeconds,
		uint32 ClientProcessId, double ClientVisualCaptureProcessSeconds,
		double ClientVisualSampleWorldSeconds, double ClientVisualSampleServerEstimateSeconds);

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