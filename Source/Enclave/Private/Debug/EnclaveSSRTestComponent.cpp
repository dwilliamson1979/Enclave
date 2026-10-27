#include "Debug/EnclaveSSRTestComponent.h"
#include "Controllers/EnclavePlayerController.h"
#include "Debug/EnclaveSSRTestSubsystem.h"
#include "Debug/EnclaveDebug.h"
#include "Characters/EnclaveCharacter.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

UEnclaveSSRTestComponent::UEnclaveSSRTestComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveSSRTestComponent::ClientEnclaveSSRTestConfigure_Implementation(int32 TestId, int32 TestRole, int32 Scenario, int32 Seed, AEnclaveCharacter* Target)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (!IsValid(PC)) return;
    if (!PC->IsLocalController() || PC->GetNetMode() != NM_Client || (TestRole != 1 && TestRole != 2) ||
        Scenario < 2 || Scenario > 5 || TestId <= 0)
    {
        ServerEnclaveSSRTestReady(TestId, TestRole, false);
        return;
    }
    PC->EnclaveDebugSSRAutoFire(0);
    PC->EnclaveDebugSSRStrafe(0);
    PC->EnclaveDebugSSRCrouch(0);
    ClientTestId = TestId;
    ClientRole = TestRole;
    ForcedTarget = Target;
    auto SetIntCVar = [](const TCHAR* Name, int32 Value) {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name)) CVar->Set(Value, ECVF_SetByCode);
    };
    SetIntCVar(TEXT("Enclave.Debug.SSRGameplayGate"), 1);
    SetIntCVar(TEXT("Enclave.Debug.SSRScenario"), Scenario);
    SetIntCVar(TEXT("Enclave.Debug.SSRAutoSeed"), Seed);
    if (IConsoleVariable* Duration = IConsoleManager::Get().FindConsoleVariable(TEXT("Enclave.Debug.SSRStrafeDuration")))
        Duration->Set(180.0f, ECVF_SetByCode);
    bool bReady = IsValid(Cast<AEnclaveCharacter>(PC->GetPawn()));
    if (TestRole == 1) bReady &= IsValid(Target) && Target->GetLocalRole() == ROLE_SimulatedProxy;
    if (bReady && TestRole == 2)
    {
        if (Scenario == 4) PC->EnclaveDebugSSRCrouch(1);
        if (Scenario != 5) PC->EnclaveDebugSSRStrafe(1);
    }
    ENCLAVE_LOG(LogEnclaveCombat, Log,
        "SSRTest CLIENT_CONFIG id=%d role=%d scenario=%d ready=%d pawn=%s target=%s (DEBUG ONLY)",
        TestId, TestRole, Scenario, bReady ? 1 : 0, *GetNameSafe(PC->GetPawn()), *GetNameSafe(Target));
    ServerEnclaveSSRTestReady(TestId, TestRole, bReady);
#else
    (void)TestId; (void)TestRole; (void)Scenario; (void)Seed; (void)Target;
#endif
}

void UEnclaveSSRTestComponent::ServerEnclaveSSRTestReady_Implementation(int32 TestId, int32 TestRole, bool bReady)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (IsValid(PC) && PC->HasAuthority())
        if (UEnclaveSSRTestSubsystem* Session = GetWorld() ? GetWorld()->GetSubsystem<UEnclaveSSRTestSubsystem>() : nullptr)
            Session->ParticipantReady(PC, TestId, TestRole, bReady);
#endif
}

void UEnclaveSSRTestComponent::ClientEnclaveSSRTestBeginFire_Implementation(int32 TestId)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (IsValid(PC) && PC->IsLocalController() && PC->GetNetMode() == NM_Client && IsCoordinatedShooter() &&
        ClientTestId == TestId && IsValid(ForcedTarget.Get()))
    {
        PC->EnclaveDebugSSRAutoFire(1);
        ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest FIRE_STARTED id=%d started=%d (DEBUG ONLY)",
            TestId, PC->bDebugSSRAutoEnabled ? 1 : 0);
    }
#else
    (void)TestId;
#endif
}

void UEnclaveSSRTestComponent::ClientEnclaveSSRTestCleanup_Implementation(int32 TestId)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(GetOwner());
    if (IsValid(PC) && PC->IsLocalController() && ClientTestId == TestId)
    {
        PC->EnclaveDebugSSRAutoFire(0);
        PC->EnclaveDebugSSRStrafe(0);
        PC->EnclaveDebugSSRCrouch(0);
        ForcedTarget.Reset();
        ClientTestId = 0;
        ClientRole = 0;
        ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest CLIENT_CLEANUP id=%d (DEBUG ONLY)", TestId);
    }
#else
    (void)TestId;
#endif
}

void UEnclaveSSRTestComponent::ParticipantUnpossessed()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    ForcedTarget.Reset();
    ClientTestId = 0;
    ClientRole = 0;
#endif
}
