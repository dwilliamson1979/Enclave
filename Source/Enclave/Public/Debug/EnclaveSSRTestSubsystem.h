#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnclaveSSRTestSubsystem.generated.h"
class AEnclavePlayerController;
class AEnclaveCharacter;
/** Non-gameplay, server-only SSR test coordinator. Does not govern hit authority. */
UCLASS()
class ENCLAVE_API UEnclaveSSRTestSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    void Start(int32 Scenario);
    void Stop(const TCHAR* Reason);
    void Status() const;
    void TickSession(float DeltaSeconds);
    void ParticipantReady(AEnclavePlayerController* Participant, int32 TestId, int32 TestRole, bool bReady);
    void ParticipantUnpossessed(AEnclavePlayerController* Participant);
    bool IsExpectedShooter(const AEnclavePlayerController* Participant, int32 InTestId) const;
private:
    int32 TestId = 0;
    int32 TestScenario = 0;
    float Elapsed = 0.f;
    int32 Phase = 0;
    bool bShooterReady = false;
    bool bTargetReady = false;
    TWeakObjectPtr<AEnclavePlayerController> HostController;
    TWeakObjectPtr<AEnclavePlayerController> ShooterController;
    TWeakObjectPtr<AEnclavePlayerController> TargetController;
    TWeakObjectPtr<AEnclaveCharacter> TargetPawn;
};
