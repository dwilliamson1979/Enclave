#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnclaveSSRTestComponent.generated.h"
class AEnclaveCharacter;
/** Owning-controller RPC endpoint for a three-player diagnostic SSR session.
 *  Never participates in production hit authority.
 *  The local strafe/GAS generators remain on PlayerController for Stage 2B.
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
private:
    int32 ClientTestId = 0;
    int32 ClientRole = 0;
    TWeakObjectPtr<AEnclaveCharacter> ForcedTarget;
};
