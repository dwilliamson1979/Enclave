#include "Debug/EnclaveSSRTestSubsystem.h"
#include "Controllers/EnclavePlayerController.h"
#include "Debug/EnclaveSSRTestComponent.h"
#include "Characters/EnclaveCharacter.h"
#include "Debug/EnclaveDebug.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerState.h"

#if ENCLAVE_INCLUDE_DEBUG_CODE
// Unattended multi-process runs (Tools/Scripts/RunSSRSession.ps1): there is no
// console to type Start into after both clients have joined.
static TAutoConsoleVariable<int32> CVarSSRTestAutoStart(
	TEXT("Enclave.Debug.SSRTest.AutoStart"), 0,
	TEXT("Listen server: start this SSR test scenario (2/3/4/5) once exactly two remote players have pawns. 0=off. DEBUG ONLY."),
	ECVF_Cheat);
static TAutoConsoleVariable<bool> CVarSSRTestAutoQuit(
	TEXT("Enclave.Debug.SSRTest.AutoQuit"), false,
	TEXT("Listen server: request engine exit when an auto-started SSR test stops. DEBUG ONLY."),
	ECVF_Cheat);
static TAutoConsoleVariable<float> CVarSSRTestDurationSeconds(
	TEXT("Enclave.Debug.SSRTest.DurationSeconds"), 180.0f,
	TEXT("Listen server: SSR test session length before it stops itself [10,600]. DEBUG ONLY."),
	ECVF_Cheat);
#endif

bool UEnclaveSSRTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    const UWorld* World = Cast<UWorld>(Outer);
    // A -game "?listen" world initializes as Standalone and only then starts
    // listening, so creation cannot require NM_ListenServer; every entry point
    // re-checks the net mode at runtime.
    return World && World->IsGameWorld() && World->GetNetMode() != NM_Client;
#else
    return false;
#endif
}

void UEnclaveSSRTestSubsystem::Start(int32 Scenario)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!GetWorld() || GetWorld()->GetNetMode() != NM_ListenServer ||
		(Scenario != 2 && Scenario != 3 && Scenario != 4 && Scenario != 5))
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START rejected=invalid_host_or_scenario scenario=%d (DEBUG ONLY)", Scenario);
		return;
	}
	HostController.Reset();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(It->Get());
		if (IsValid(PC) && PC->HasAuthority() && PC->IsLocalController()) { HostController = PC; break; }
	}
	if (!HostController.IsValid()) return;
	if (Phase != 0)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START rejected=already_active use_Stop_first (DEBUG ONLY)");
		return;
	}
	TArray<AEnclavePlayerController*> Remotes;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(It->Get());
		if (IsValid(PC) && PC != HostController.Get() && PC->HasAuthority() && !PC->IsLocalController() &&
			IsValid(PC->GetPawn()) && IsValid(PC->PlayerState) && IsValid(PC->GetSSRTestComponent()))
		{
			Remotes.Add(PC);
		}
	}
	if (Remotes.Num() != 2)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START rejected=requires_exactly_two_remote_controllers found=%d (DEBUG ONLY)", Remotes.Num());
		return;
	}
	Remotes.Sort([](const AEnclavePlayerController& A, const AEnclavePlayerController& B) {
		return A.PlayerState->GetPlayerId() < B.PlayerState->GetPlayerId();
	});
	AEnclaveCharacter* SelectedTargetPawn = Cast<AEnclaveCharacter>(Remotes[1]->GetPawn());
	if (!IsValid(SelectedTargetPawn) || !IsValid(Cast<AEnclaveCharacter>(Remotes[0]->GetPawn())))
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START rejected=missing_characters (DEBUG ONLY)");
		return;
	}
	if (IConsoleVariable* Gate = IConsoleManager::Get().FindConsoleVariable(TEXT("Enclave.Debug.SSRGameplayGate"))) Gate->Set(true, ECVF_SetByCode);
	bAutoStarted = false;
	TestId++;
	if (TestId <= 0) TestId = 1;
	TestScenario = Scenario;
	Elapsed = 0.0f;
	Phase = 1;
	bTargetReady = false;
	bShooterReady = false;
	ShooterController = Remotes[0];
	TargetController = Remotes[1];
	TargetPawn = SelectedTargetPawn;
	SelectedTargetPawn->DebugSetSSRStrafeDamageSuppressed(true);
	Remotes[1]->GetSSRTestComponent()->ClientEnclaveSSRTestConfigure(TestId, 2, Scenario, 1729, nullptr);
	Remotes[0]->GetSSRTestComponent()->ClientEnclaveSSRTestConfigure(TestId, 1, Scenario, 1729, SelectedTargetPawn);
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRTest START id=%d scenario=%d shooterPlayerId=%d targetPlayerId=%d shooter=%s target=%s (SHADOW ONLY)",
		TestId, Scenario, Remotes[0]->PlayerState->GetPlayerId(), Remotes[1]->PlayerState->GetPlayerId(),
		*GetNameSafe(Remotes[0]->GetPawn()), *GetNameSafe(SelectedTargetPawn));
#else
	(void)Scenario;
#endif
}

void UEnclaveSSRTestSubsystem::Stop(const TCHAR* Reason)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!GetWorld() || GetWorld()->GetNetMode() != NM_ListenServer || Phase == 0) return;
	const int32 OldId = TestId;
	if (AEnclavePlayerController* Shooter = ShooterController.Get()) Shooter->GetSSRTestComponent()->ClientEnclaveSSRTestCleanup(OldId);
	if (AEnclavePlayerController* Target = TargetController.Get()) Target->GetSSRTestComponent()->ClientEnclaveSSRTestCleanup(OldId);
	if (AEnclaveCharacter* ProtectedTargetPawn = TargetPawn.Get()) ProtectedTargetPawn->DebugSetSSRStrafeDamageSuppressed(false);
	ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest STOP id=%d reason=%s phase=%d elapsed=%.1f (DEBUG ONLY)",
		OldId, Reason, Phase, Elapsed);
	Phase = 0;
	if (bAutoStarted && CVarSSRTestAutoQuit.GetValueOnGameThread())
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest AUTOQUIT id=%d (DEBUG ONLY)", OldId);
		FPlatformMisc::RequestExit(false, TEXT("SSRTest.AutoQuit"));
	}
	bAutoStarted = false;
	ShooterController.Reset();
	TargetController.Reset();
	TargetPawn.Reset();
	bTargetReady = false;
	bShooterReady = false;
#endif
}

void UEnclaveSSRTestSubsystem::Status() const
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRTest STATUS id=%d phase=%d scenario=%d shooter=%s target=%s shooterReady=%d targetReady=%d elapsed=%.1f (DEBUG ONLY)",
		TestId, Phase, TestScenario, *GetNameSafe(ShooterController.Get()),
		*GetNameSafe(TargetController.Get()), bShooterReady ? 1 : 0,
		bTargetReady ? 1 : 0, Elapsed);
#endif
}

void UEnclaveSSRTestSubsystem::TickSession(float DeltaSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!GetWorld() || GetWorld()->GetNetMode() != NM_ListenServer) return;
	if (Phase == 0)
	{
		TickAutoStart(DeltaSeconds);
		return;
	}
	Elapsed += DeltaSeconds;
	const float DurationSeconds = FMath::Clamp(CVarSSRTestDurationSeconds.GetValueOnGameThread(), 10.0f, 600.0f);
	if (Elapsed >= DurationSeconds || !ShooterController.IsValid() ||
		!TargetController.IsValid() || !TargetPawn.IsValid())
	{
		Stop(TEXT("timeout_or_participant_lost"));
		return;
	}
	if (Phase == 1 && bShooterReady && bTargetReady)
	{
		Phase = 2;
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest CONFIGURED id=%d waiting_target_motion=1 (DEBUG ONLY)", TestId);
	}
    if (Phase == 3)
    {
        if (AEnclavePlayerController* Shooter = ShooterController.Get())
            if (UEnclaveSSRTestComponent* Component = Shooter->GetSSRTestComponent())
                Component->TickServerViewReadiness(TestId);
    }
	if (Phase == 2 && Elapsed >= 2.5f)
	{
		AEnclaveCharacter* Target = TargetPawn.Get();
		const float Speed = Target ? Target->GetVelocity().Size2D() : 0.0f;
		if (TestScenario == 5 || Speed >= 30.0f)
		{
			Phase = 3;
			if (AEnclavePlayerController* Shooter = ShooterController.Get()) Shooter->GetSSRTestComponent()->ClientEnclaveSSRTestBeginFire(TestId);
			ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest FIRING id=%d targetServerSpeedCmS=%.1f (DEBUG ONLY)", TestId, Speed);
		}
		else if (Elapsed > 12.0f)
		{
			Stop(TEXT("target_movement_not_verified"));
		}
	}
#else
	(void)DeltaSeconds;
#endif
}

void UEnclaveSSRTestSubsystem::TickAutoStart(float DeltaSeconds)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	const int32 Scenario = CVarSSRTestAutoStart.GetValueOnGameThread();
	if (Scenario == 0 || bAutoStartConsumed) return;
	// Poll once per second and stay quiet until the lobby is complete; Start()
	// logs its own rejection reasons once the participant count is right.
	AutoStartPollSeconds += DeltaSeconds;
	if (AutoStartPollSeconds < 1.0f) return;
	AutoStartPollSeconds = 0.0f;
	int32 RemotesWithPawns = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (IsValid(PC) && !PC->IsLocalController() && IsValid(PC->GetPawn())) ++RemotesWithPawns;
	}
	if (RemotesWithPawns != 2) return;
	// Let initial replication and clock sync settle before measuring timing.
	AutoStartSettleSeconds += 1.0f;
	if (AutoStartSettleSeconds < 5.0f) return;
	bAutoStartConsumed = true;
	Start(Scenario);
	bAutoStarted = Phase != 0;
	ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest AUTOSTART scenario=%d started=%d (DEBUG ONLY)", Scenario, bAutoStarted ? 1 : 0);
	if (!bAutoStarted && CVarSSRTestAutoQuit.GetValueOnGameThread())
	{
		FPlatformMisc::RequestExit(false, TEXT("SSRTest.AutoQuit"));
	}
#else
	(void)DeltaSeconds;
#endif
}

bool UEnclaveSSRTestSubsystem::IsExpectedShooter(const AEnclavePlayerController* Participant, int32 InTestId) const
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    return Phase == 3 && TestId == InTestId && ShooterController.Get() == Participant;
#else
    return false;
#endif
}

void UEnclaveSSRTestSubsystem::ParticipantReady(AEnclavePlayerController* Participant, int32 InTestId, int32 TestRole, bool bReady)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    if (Phase != 1 || TestId != InTestId || !IsValid(Participant)) return;
    if ((TestRole == 1 && ShooterController.Get() != Participant) ||
        (TestRole == 2 && TargetController.Get() != Participant) || (TestRole != 1 && TestRole != 2)) return;
    if (!bReady) { Stop(TEXT("client_not_ready")); return; }
    if (TestRole == 1) bShooterReady = true;
    else bTargetReady = true;
    ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest READY id=%d role=%d (DEBUG ONLY)", TestId, TestRole);
#endif
}

void UEnclaveSSRTestSubsystem::ParticipantUnpossessed(AEnclavePlayerController* Participant)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
    if (Phase && (Participant == HostController.Get() || Participant == ShooterController.Get() || Participant == TargetController.Get()))
        Stop(TEXT("participant_unpossessed"));
#endif
}
