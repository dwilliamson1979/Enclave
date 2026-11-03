// Copyright Lightcarrier Studios

#include "Game/EnclaveMatchGameMode.h"

#include "Debug/EnclaveDebug.h"
#include "Game/EnclaveMatchGameState.h"
#include "Player/EnclaveMatchPlayerState.h"
#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"

AEnclaveMatchGameMode::AEnclaveMatchGameMode()
{
	GameStateClass = AEnclaveMatchGameState::StaticClass();
	PlayerStateClass = AEnclaveMatchPlayerState::StaticClass();
}

void AEnclaveMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MatchSettings.ApplyOptions(Options);
	UE_LOG(LogEnclaveMatch, Log, TEXT("Match settings: %s"), *MatchSettings.ToString());
}

bool AEnclaveMatchGameMode::ReadyToStartMatch_Implementation()
{
	AEnclaveMatchGameState* MatchGameState = GetGameState<AEnclaveMatchGameState>();
	if (!ensureMsgf(MatchGameState, TEXT("AEnclaveMatchGameMode needs an AEnclaveMatchGameState; the match can't start.")))
	{
		return false;
	}

	// Super is false with no players at all (or while bDelayedStart is set), so
	// it counts as "not enough players" too: the countdown must cancel in that
	// case, or a stale end time could start the match the moment players return.
	// Keep this ahead of any early return.
	const bool bEnoughPlayers = Super::ReadyToStartMatch_Implementation()
		&& GetNumPlayers() >= MatchSettings.MinPlayersToStart;
	if (!bEnoughPlayers)
	{
		CancelWarmup(*MatchGameState);
		return false;
	}

	// The server clock that clients also see, so the HUD countdown matches.
	const double Now = MatchGameState->GetServerWorldTimeSeconds();

	if (!WarmupEndServerTime.IsSet())
	{
		WarmupEndServerTime = Now + MatchSettings.WarmupSeconds;
		MatchGameState->SetPhaseEndServerTime(*WarmupEndServerTime);
		UE_LOG(LogEnclaveMatch, Log, TEXT("Warmup started: %d players, match starts in %d s."),
			GetNumPlayers(), MatchSettings.WarmupSeconds);
	}

	return Now >= *WarmupEndServerTime;
}

void AEnclaveMatchGameMode::HandleMatchHasStarted()
{
	// The warmup is over; don't leave its end time behind as stale state.
	WarmupEndServerTime.Reset();

	AEnclaveMatchGameState* MatchGameState = GetGameState<AEnclaveMatchGameState>();
	if (ensureMsgf(MatchGameState, TEXT("AEnclaveMatchGameMode needs an AEnclaveMatchGameState; the time limit was not set.")))
	{
		// The phase end now means the time limit (0 = none, so the HUD shows no timer).
		const double Now = MatchGameState->GetServerWorldTimeSeconds();
		const double MatchEnd = MatchSettings.TimeLimitSeconds > 0 ? Now + MatchSettings.TimeLimitSeconds : 0.0;
		MatchGameState->SetPhaseEndServerTime(MatchEnd);
	}

	UE_LOG(LogEnclaveMatch, Log, TEXT("Match started: %d players, time limit %d s, score limit %d."),
		GetNumPlayers(), MatchSettings.TimeLimitSeconds, MatchSettings.ScoreLimit);

	// Last: Super spawns every player, and they must spawn into a fresh match.
	Super::HandleMatchHasStarted();
}

void AEnclaveMatchGameMode::CancelWarmup(AEnclaveMatchGameState& MatchGameState)
{
	// Called every frame while waiting, so only act (and log) on a change.
	if (!WarmupEndServerTime.IsSet())
	{
		return;
	}

	WarmupEndServerTime.Reset();
	MatchGameState.SetPhaseEndServerTime(0.0);
	UE_LOG(LogEnclaveMatch, Log, TEXT("Warmup cancelled: %d of %d players."),
		GetNumPlayers(), MatchSettings.MinPlayersToStart);
}

void AEnclaveMatchGameMode::SetPlayerDefaults(APawn* Pawn)
{
	Super::SetPlayerDefaults(Pawn);

	if (!IsValid(Pawn))
	{
		return;
	}

	UEnclaveAvatarLifecycleComponent* LifecycleComponent = Pawn->FindComponentByClass<UEnclaveAvatarLifecycleComponent>();
	if (IsValid(LifecycleComponent))
	{
		LifecycleComponent->OnAvatarDied.AddUniqueDynamic(this, &ThisClass::HandleAvatarDied);
	}
}

void AEnclaveMatchGameMode::HandleAvatarDied(UEnclaveAvatarLifecycleComponent* LifecycleComp, FEnclaveDamageRequest DamageRequest)
{
	if (!IsMatchInProgress() || !IsValid(LifecycleComp))
	{
		return;
	}

	APawn* Victim = LifecycleComp->GetOwner<APawn>();
	AEnclaveMatchPlayerState* VictimPlayerState = Victim ? Victim->GetPlayerState<AEnclaveMatchPlayerState>() : nullptr;
	if(!IsValid(Victim) || !IsValid(VictimPlayerState))
	{
		return;
	}
	
	VictimPlayerState->AddDeath();

	APawn* Attacker = Cast<APawn>(DamageRequest.SourceActor.Get());
	AEnclaveMatchPlayerState* AttackerPlayerState = Attacker ? Attacker->GetPlayerState<AEnclaveMatchPlayerState>() : nullptr;
	if (AttackerPlayerState && AttackerPlayerState->IsHostileTo(VictimPlayerState))
	{
		AttackerPlayerState->AddKill();
	}
	
	ScoreKill(AttackerPlayerState, VictimPlayerState);

	AEnclaveMatchGameState* MatchGameState = GetGameState<AEnclaveMatchGameState>();
	if (IsValid(MatchGameState))
	{
		MatchGameState->MulticastKill(AttackerPlayerState, VictimPlayerState);
	}

	FTimerHandle RespawnTimerHandle;
	GetWorldTimerManager().SetTimer(RespawnTimerHandle, FTimerDelegate::CreateUObject(this, &AEnclaveMatchGameMode::RespawnPlayer, TWeakObjectPtr<AController>(Victim->GetController())), MatchSettings.RespawnDelaySeconds, false);
}

void AEnclaveMatchGameMode::RespawnPlayer(TWeakObjectPtr<AController> Controller)
{
	if (!IsMatchInProgress() || !Controller.IsValid())
	{
		return;
	}

	APawn* Pawn = Controller->GetPawn();

	//Call an interface method on the pawn that performs the cleanup? Or look for a component which handles things like respawn?

	if(IsValid(Pawn))
	{
		Pawn->Destroy();
	}

	RestartPlayer(Controller.Get());
}