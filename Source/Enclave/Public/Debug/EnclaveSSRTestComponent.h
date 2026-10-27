#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "Engine/NetSerialization.h"
#include "EnclaveSSRTestComponent.generated.h"
class AEnclaveCharacter;
/** Owning-controller RPC endpoint for a three-player diagnostic SSR session.
 *  Never participates in production hit authority.
 *  All local strafe/crouch/GAS generators live here; they use normal movement and input.
 */
UCLASS(ClassGroup=(EnclaveDebug), NotBlueprintable, meta=(BlueprintSpawnableComponent=false))
class ENCLAVE_API UEnclaveSSRTestComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEnclaveSSRTestComponent();
    UFUNCTION(Client, Reliable)
    void ClientEnclaveSSRTestConfigure(int32 TestId, int32 TestRole, int32 Scenario, int32 Seed, AEnclaveCharacter* Target);
    UFUNCTION(Server, Reliable)
    void ServerEnclaveSSRTestReady(int32 TestId, int32 TestRole, bool bReady);
    UFUNCTION(Client, Reliable)
    void ClientEnclaveSSRTestBeginFire(int32 TestId);
    UFUNCTION(Client, Reliable)
    void ClientEnclaveSSRTestCleanup(int32 TestId);
    bool IsCoordinatedShooter() const { return ClientRole == 1 && ClientTestId > 0; }
    AEnclaveCharacter* GetForcedTarget() const { return ForcedTarget.Get(); }
    void ParticipantUnpossessed();
    void EnclaveDebugSSRStrafe(int32 bEnable);
    void EnclaveDebugSSRCrouch(int32 bEnable);
    void EnclaveDebugSSRAutoFire(int32 bEnable);
    void TickParticipant(float DeltaSeconds);
    bool IsAutoFireActive() const { return bDebugSSRAutoEnabled; }
    // Server-driven diagnostic alignment; does not authorize gameplay shots.
    void TickServerViewReadiness(int32 ActiveTestId);
    UFUNCTION(Server, Reliable)
    void ServerEnclaveSSRTestRequestViewReady(int32 TestId, FVector_NetQuantizeNormal AimDirection);
    UFUNCTION(Client, Reliable)
    void ClientEnclaveSSRTestViewReady(int32 TestId);


private:
    friend class AEnclavePlayerController;
    // C4E test rig state is local to the owning component; never replicated.
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


    bool bAwaitingViewReady = false;
    int32 ServerViewTestId = 0;
    FVector ServerViewRequestedDirection = FVector::ZeroVector;
    double ServerViewRequestTime = 0.0;
    int32 ClientTestId = 0;
    int32 ClientRole = 0;
    TWeakObjectPtr<AEnclaveCharacter> ForcedTarget;
};
