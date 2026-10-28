#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "EnclaveSSRTestSubsystem.generated.h"
class AEnclavePlayerController;
class UNetConnection;
class AEnclaveCharacter;
/** Non-gameplay, server-only SSR test coordinator. Does not govern hit authority.
 * Runs on a listen server (host present) or a dedicated server (no host);
 * it ticks itself so neither case depends on a local player controller.
 */
UCLASS()
class ENCLAVE_API UEnclaveSSRTestSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Tick(float DeltaTime) override { TickSession(DeltaTime); }
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UEnclaveSSRTestSubsystem, STATGROUP_Tickables); }
    void Start(int32 Scenario);
    void Stop(const TCHAR* Reason);
    void Status() const;
    void TickSession(float DeltaSeconds);
    void ParticipantReady(AEnclavePlayerController* Participant, int32 TestId, int32 TestRole, bool bReady);
    void ParticipantUnpossessed(AEnclavePlayerController* Participant);
    bool IsExpectedShooter(const AEnclavePlayerController* Participant, int32 InTestId) const;
private:
    void TickAutoStart(float DeltaSeconds);
    void LogNetStats(const TCHAR* Label);
    struct FNetSample
    {
        TWeakObjectPtr<UNetConnection> Connection;
        FString Player;
        int64 OutBytes = 0, InBytes = 0, OutPackets = 0, InPackets = 0, OutLost = 0, InLost = 0;
    };
    TArray<FNetSample> NetStart;
    int32 SessionTicks = 0;
    bool IsServerWorld() const;
    int32 TestId = 0;
    int32 TestScenario = 0;
    float Elapsed = 0.f;
    int32 Phase = 0;
    bool bShooterReady = false;
    bool bTargetReady = false;
    bool bAutoStarted = false;
    bool bAutoStartConsumed = false;
    float AutoStartPollSeconds = 0.f;
    float AutoStartSettleSeconds = 0.f;
    TWeakObjectPtr<AEnclavePlayerController> HostController;
    TWeakObjectPtr<AEnclavePlayerController> ShooterController;
    TWeakObjectPtr<AEnclavePlayerController> TargetController;
    TWeakObjectPtr<AEnclaveCharacter> TargetPawn;
    // Extra remote players: they move like the target and are damage-protected.
    TArray<TWeakObjectPtr<AEnclavePlayerController>> Bystanders;
};
