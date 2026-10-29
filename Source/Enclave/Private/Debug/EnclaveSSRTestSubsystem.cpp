#include "Debug/EnclaveSSRTestSubsystem.h"
#include "Controllers/EnclavePlayerController.h"
#include "Debug/EnclaveSSRTestComponent.h"
#include "Characters/EnclaveCharacter.h"
#include "Debug/EnclaveDebug.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerState.h"
#include "Engine/NetConnection.h"
#include "Engine/TargetPoint.h"
#include "EngineUtils.h"

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
static TAutoConsoleVariable<int32> CVarSSRTestExpectedClients(
	TEXT("Enclave.Debug.SSRTest.ExpectedClients"), 2,
	TEXT("Remote players AutoStart waits for (>= 2). Lowest PlayerId shoots, next is the target, the rest are bystanders. DEBUG ONLY."),
	ECVF_Cheat);
static TAutoConsoleVariable<FString> CVarSSRTestLayout(
	TEXT("Enclave.Debug.SSRTest.Layout"), TEXT(""),
	TEXT("Test-map layout (open, crossing, backstop, cover, halfcover, pillars, crowd): teleport participants to SSR.<Layout>.* markers before the session. Empty = stay where spawned. DEBUG ONLY."),
	ECVF_Cheat);
static TAutoConsoleVariable<float> CVarSSRTestTeleportPeriod(
	TEXT("Enclave.Debug.SSRTest.TeleportPeriod"), 0.0f,
	TEXT("Server: teleport the target 150 cm sideways (alternating) every N seconds while firing (0 = off). History-discontinuity test. DEBUG ONLY."),
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

bool UEnclaveSSRTestSubsystem::IsServerWorld() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetNetMode() == NM_ListenServer || World->GetNetMode() == NM_DedicatedServer);
}

void UEnclaveSSRTestSubsystem::Start(int32 Scenario)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsServerWorld() || (Scenario != 2 && Scenario != 3 && Scenario != 4 && Scenario != 5))
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START rejected=invalid_host_or_scenario scenario=%d (DEBUG ONLY)", Scenario);
		return;
	}
	// A listen server must have its host; a dedicated server has none.
	HostController.Reset();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AEnclavePlayerController* PC = Cast<AEnclavePlayerController>(It->Get());
		if (IsValid(PC) && PC->HasAuthority() && PC->IsLocalController()) { HostController = PC; break; }
	}
	if (GetWorld()->GetNetMode() == NM_ListenServer && !HostController.IsValid()) return;
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
	if (Remotes.Num() < 2)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest START rejected=requires_at_least_two_remote_controllers found=%d (DEBUG ONLY)", Remotes.Num());
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
	bTargetReady = false;
	bShooterReady = false;
	ShooterController = Remotes[0];
	TargetController = Remotes[1];
	TargetPawn = SelectedTargetPawn;
	SelectedTargetPawn->DebugSetSSRStrafeDamageSuppressed(true);
	Bystanders.Reset();
	for (int32 Index = 2; Index < Remotes.Num(); ++Index)
	{
		Bystanders.Add(Remotes[Index]);
		if (AEnclaveCharacter* BystanderPawn = Cast<AEnclaveCharacter>(Remotes[Index]->GetPawn()))
		{
			BystanderPawn->DebugSetSSRStrafeDamageSuppressed(true);
		}
	}
	const FString Layout = CVarSSRTestLayout.GetValueOnGameThread();
	if (Layout.IsEmpty())
	{
		ConfigureParticipants();
	}
	else if (PlaceParticipants(Layout))
	{
		// Clients capture their strafe origin and axis when configured, so let the
		// teleport and the new control rotation replicate first.
		Phase = 4;
	}
	else
	{
		Phase = 1; // Lets Stop() clean up protection and bystanders.
		Stop(TEXT("layout_markers_missing"));
		return;
	}
	LogNetStats(TEXT("start"));
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"SSRTest START id=%d scenario=%d shooterPlayerId=%d targetPlayerId=%d shooter=%s target=%s bystanders=%d (SHADOW ONLY)",
		TestId, Scenario, Remotes[0]->PlayerState->GetPlayerId(), Remotes[1]->PlayerState->GetPlayerId(),
		*GetNameSafe(Remotes[0]->GetPawn()), *GetNameSafe(SelectedTargetPawn), Bystanders.Num());
#else
	(void)Scenario;
#endif
}

void UEnclaveSSRTestSubsystem::Stop(const TCHAR* Reason)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsServerWorld() || Phase == 0) return;
	const int32 OldId = TestId;
	LogNetStats(TEXT("stop"));
	if (AEnclavePlayerController* Shooter = ShooterController.Get()) Shooter->GetSSRTestComponent()->ClientEnclaveSSRTestCleanup(OldId);
	if (AEnclavePlayerController* Target = TargetController.Get()) Target->GetSSRTestComponent()->ClientEnclaveSSRTestCleanup(OldId);
	if (AEnclaveCharacter* ProtectedTargetPawn = TargetPawn.Get()) ProtectedTargetPawn->DebugSetSSRStrafeDamageSuppressed(false);
	for (const TWeakObjectPtr<AEnclavePlayerController>& Bystander : Bystanders)
	{
		if (AEnclavePlayerController* PC = Bystander.Get())
		{
			if (AEnclaveCharacter* BystanderPawn = Cast<AEnclaveCharacter>(PC->GetPawn())) BystanderPawn->DebugSetSSRStrafeDamageSuppressed(false);
			PC->GetSSRTestComponent()->ClientEnclaveSSRTestCleanup(OldId);
		}
	}
	Bystanders.Reset();
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
	if (!IsServerWorld()) return;
	if (Phase == 0)
	{
		TickAutoStart(DeltaSeconds);
		return;
	}
	Elapsed += DeltaSeconds;
	++SessionTicks;
	if (Phase == 4 && Elapsed >= 1.0f)
	{
		ConfigureParticipants();
	}
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
        const float TeleportPeriod = CVarSSRTestTeleportPeriod.GetValueOnGameThread();
        AEnclaveCharacter* Target = TargetPawn.Get();
        if (TeleportPeriod > 0.0f && IsValid(Target) && (EdgeTeleportElapsed += DeltaSeconds) >= TeleportPeriod)
        {
            EdgeTeleportElapsed = 0.0f;
            EdgeTeleportSign = -EdgeTeleportSign;
            const FVector Offset = Target->GetActorRightVector() * 150.0f * EdgeTeleportSign;
            // TeleportTo marks the history discontinuity.
            const bool bMoved = Target->TeleportTo(Target->GetActorLocation() + Offset, Target->GetActorRotation());
            ENCLAVE_LOG(LogEnclaveCombat, Log, "SSREdge SERVER teleport=%d offsetCm=%.0f (DEBUG ONLY)", bMoved ? 1 : 0, 150.0f * EdgeTeleportSign);
        }
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
	if (RemotesWithPawns < FMath::Max(2, CVarSSRTestExpectedClients.GetValueOnGameThread())) return;
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

void UEnclaveSSRTestSubsystem::LogNetStats(const TCHAR* Label)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	// Cumulative connection counters: snapshot at start, log the session delta at stop.
	const bool bStart = FCString::Strcmp(Label, TEXT("start")) == 0;
	if (bStart)
	{
		NetStart.Reset();
		SessionTicks = 0;
	}
	else
	{
		ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest TICKRATE id=%d serverTickHz=%.1f ticks=%d seconds=%.1f (DEBUG ONLY)",
			TestId, SessionTicks / FMath::Max(Elapsed, 0.001f), SessionTicks, Elapsed);
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		UNetConnection* Connection = IsValid(PC) ? PC->GetNetConnection() : nullptr;
		if (!IsValid(Connection)) continue;
		FNetSample Now;
		Now.Connection = Connection;
		Now.Player = PC->PlayerState ? FString::FromInt(PC->PlayerState->GetPlayerId()) : TEXT("?");
		Now.OutBytes = Connection->OutTotalBytes;
		Now.InBytes = Connection->InTotalBytes;
		Now.OutPackets = Connection->OutTotalPackets;
		Now.InPackets = Connection->InTotalPackets;
		Now.OutLost = Connection->OutTotalPacketsLost;
		Now.InLost = Connection->InTotalPacketsLost;
		if (bStart)
		{
			NetStart.Add(Now);
			continue;
		}
		const FNetSample* Start = NetStart.FindByPredicate([Connection](const FNetSample& S) { return S.Connection.Get() == Connection; });
		if (!Start) continue;
		const double Seconds = FMath::Max(Elapsed, 0.001f);
		ENCLAVE_LOG(LogEnclaveCombat, Log,
			"SSRTest NETSTATS id=%d playerId=%s seconds=%.1f serverOutBytesPerSec=%.0f serverInBytesPerSec=%.0f "
			"outPackets=%lld inPackets=%lld outLost=%lld inLost=%lld (DEBUG ONLY)",
			TestId, *Now.Player, Seconds, (Now.OutBytes - Start->OutBytes) / Seconds, (Now.InBytes - Start->InBytes) / Seconds,
			Now.OutPackets - Start->OutPackets, Now.InPackets - Start->InPackets,
			Now.OutLost - Start->OutLost, Now.InLost - Start->InLost);
	}
#else
	(void)Label;
#endif
}

void UEnclaveSSRTestSubsystem::ConfigureParticipants()
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	Phase = 1;
	if (AEnclavePlayerController* Target = TargetController.Get())
	{
		Target->GetSSRTestComponent()->ClientEnclaveSSRTestConfigure(TestId, 2, TestScenario, 1729, nullptr);
	}
	for (const TWeakObjectPtr<AEnclavePlayerController>& Bystander : Bystanders)
	{
		if (AEnclavePlayerController* PC = Bystander.Get())
		{
			PC->GetSSRTestComponent()->ClientEnclaveSSRTestConfigure(TestId, 3, TestScenario, 1729, nullptr);
		}
	}
	if (AEnclavePlayerController* Shooter = ShooterController.Get())
	{
		Shooter->GetSSRTestComponent()->ClientEnclaveSSRTestConfigure(TestId, 1, TestScenario, 1729, TargetPawn.Get());
	}
#endif
}

bool UEnclaveSSRTestSubsystem::PlaceParticipants(const FString& Layout)
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	TMap<FName, const ATargetPoint*> Markers;
	for (TActorIterator<ATargetPoint> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			Markers.Add(Tag, *It);
		}
	}
	const auto Find = [&Markers](const FString& Tag) -> const ATargetPoint*
	{
		const ATargetPoint* const* Found = Markers.Find(FName(*Tag));
		return Found ? *Found : nullptr;
	};
	const auto Place = [this](AEnclavePlayerController* PC, const ATargetPoint* Marker)
	{
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!IsValid(Pawn) || !Marker) return false;
		const FRotator Facing(0.0f, Marker->GetActorRotation().Yaw, 0.0f);
		// TeleportTo marks a history discontinuity on Enclave characters.
		const bool bMoved = Pawn->TeleportTo(Marker->GetActorLocation(), Facing);
		PC->ClientSetRotation(Facing);
		PC->SetControlRotation(Facing);
		return bMoved;
	};

	const ATargetPoint* ShooterMarker = Find(FString::Printf(TEXT("SSR.%s.Shooter"), *Layout));
	const ATargetPoint* TargetMarker = Find(FString::Printf(TEXT("SSR.%s.Target"), *Layout));
	if (!ShooterMarker || !TargetMarker)
	{
		ENCLAVE_LOG(LogEnclaveCombat, Warning, "SSRTest LAYOUT rejected layout=%s reason=missing_shooter_or_target_marker (DEBUG ONLY)", *Layout);
		return false;
	}
	bool bPlaced = Place(ShooterController.Get(), ShooterMarker) && Place(TargetController.Get(), TargetMarker);
	int32 InLayout = 0;
	int32 Parked = 0;
	for (int32 Index = 0; Index < Bystanders.Num(); ++Index)
	{
		const ATargetPoint* Marker = Find(FString::Printf(TEXT("SSR.%s.Bystander.%d"), *Layout, Index + 1));
		if (Marker) { ++InLayout; }
		else
		{
			// Bystanders the layout has no place for wait out of every lane.
			Marker = Find(FString::Printf(TEXT("SSR.Holding.%d"), ++Parked));
		}
		bPlaced &= Place(Bystanders[Index].Get(), Marker);
	}
	ENCLAVE_LOG(LogEnclaveCombat, Log, "SSRTest LAYOUT layout=%s placed=%d bystandersInLayout=%d bystandersParked=%d (DEBUG ONLY)",
		*Layout, bPlaced ? 1 : 0, InLayout, Parked);
	return bPlaced;
#else
	(void)Layout;
	return false;
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
