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

	// Restarts (RestartGame's ?Restart) keep clients connected, and each
	// PlayerState is copied into the next match (CopyProperties), so a team
	// mode can keep teams. With no transition map set, the engine travels
	// through an empty dummy world (World.cpp, FSeamlessTravelHandler::StartTravel).
	bUseSeamlessTravel = true;
}

void AEnclaveMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	MatchSettings.ApplyOptions(Options);
	UE_LOG(LogEnclaveMatch, Log, TEXT("Match settings: %s"), *MatchSettings.ToString());

	// Operator input (URL or Blueprint), so warn rather than refuse: an endless
	// practice server is legitimate. ApplyOptions checks values one at a time;
	// this is a combination.
	if (MatchSettings.ScoreLimit == 0 && MatchSettings.TimeLimitSeconds == 0)
	{
		UE_LOG(LogEnclaveMatch, Warning,
			TEXT("No score limit and no time limit: the match only ends if the mode has its own end rule."));
	}
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

	// A match starts at zero. A seamless restart usually arrives with zeroed
	// stats (the engine resets the PlayerState before copying it), but not when
	// the player controller class changes between modes, so don't rely on it.
	// Inactive players too: one who left during warmup and comes back resumes
	// their PlayerState.
	auto ResetStats = [](const TArray<TObjectPtr<APlayerState>>& Players)
	{
		for (APlayerState* Player : Players)
		{
			if (AEnclaveMatchPlayerState* MatchPlayer = Cast<AEnclaveMatchPlayerState>(Player))
			{
				MatchPlayer->ResetMatchStats();
			}
		}
	};
	if (GameState)
	{
		ResetStats(GameState->PlayerArray);
	}
	ResetStats(InactivePlayerArray);

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

bool AEnclaveMatchGameMode::ReadyToEndMatch_Implementation()
{
	// Polled rather than checked on events: one place decides the end, whatever
	// changed. A few comparisons per frame; negligible.
	AEnclaveMatchGameState* MatchGameState = GetGameState<AEnclaveMatchGameState>();
	if (!ensureMsgf(MatchGameState, TEXT("AEnclaveMatchGameMode needs an AEnclaveMatchGameState; the match can't end.")))
	{
		return false;
	}

	// Every match has a time limit (0 = none), so the base owns it. Who won is
	// the branch's question. Adding time later only moves PhaseEndServerTime.
	const double PhaseEnd = MatchGameState->GetPhaseEndServerTime();
	const bool bTimeLimitReached = MatchSettings.TimeLimitSeconds > 0
		&& PhaseEnd > 0.0
		&& MatchGameState->GetServerWorldTimeSeconds() >= PhaseEnd;

	return CheckWinCondition(bTimeLimitReached);
}

void AEnclaveMatchGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	UE_LOG(LogEnclaveMatch, Log, TEXT("Match ended; next match in %d s."), MatchSettings.PostMatchSeconds);

	// The phase end now means "next match starts" (0 = restarting now). Without a
	// GameState the HUD has no countdown, but the server must still restart.
	AEnclaveMatchGameState* MatchGameState = GetGameState<AEnclaveMatchGameState>();
	if (ensureMsgf(MatchGameState, TEXT("AEnclaveMatchGameMode needs an AEnclaveMatchGameState; the post-match countdown was not set.")))
	{
		const double Now = MatchGameState->GetServerWorldTimeSeconds();
		const double NextMatch = MatchSettings.PostMatchSeconds > 0 ? Now + MatchSettings.PostMatchSeconds : 0.0;
		MatchGameState->SetPhaseEndServerTime(NextMatch);
	}

	// A timer with a delay of 0 never fires, so restart directly.
	if (MatchSettings.PostMatchSeconds <= 0)
	{
		RestartGame();
		return;
	}

	// The travel discards this GameMode and its timers, so a local handle is enough.
	FTimerHandle PostMatchTimer;
	GetWorldTimerManager().SetTimer(PostMatchTimer, this, &ThisClass::RestartGame, MatchSettings.PostMatchSeconds, false);
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

EEnclaveHitResponse AEnclaveMatchGameMode::ResolveDamage(const AActor* Target, FEnclaveDamageRequest& InOutRequest) const
{
	// Not a player (a dummy, a destructible): its own code decides what damage does.
	const APawn* VictimPawn = Cast<APawn>(Target);
	const AEnclavePlayerState* Victim = VictimPawn ? VictimPawn->GetPlayerState<AEnclavePlayerState>() : nullptr;
	if (!Victim)
	{
		return EEnclaveHitResponse::Apply;
	}

	const FEnclaveHitPolicy& Policy = GetCurrentHitPolicy();

	// No player behind the damage (falling, fire), or one who has left.
	const AEnclavePlayerState* Attacker = Cast<AEnclavePlayerState>(InOutRequest.SourcePlayerState.Get());
	if (!IsValid(Attacker))
	{
		return Policy.Environment;
	}

	if (Attacker == Victim)
	{
		return Policy.Self;
	}

	switch (Attacker->GetAttitudeTowards(Victim))
	{
	case EEnclaveAttitude::Hostile:
		return Policy.Hostile;
	case EEnclaveAttitude::Friendly:
		if (Policy.Friendly != EEnclaveHitResponse::Ignore)
		{
			InOutRequest.Damage *= Policy.FriendlyDamageScale;
		}
		return Policy.Friendly;
	case EEnclaveAttitude::Neutral:
	default:
		return EEnclaveHitResponse::Ignore;
	}
}

void AEnclaveMatchGameMode::OnMatchStateSet()
{
	Super::OnMatchStateSet();

	// Leaving the map (or aborting) keeps the last phase's policy.
	if (MatchState == MatchState::EnteringMap || MatchState == MatchState::WaitingToStart)
	{
		CurrentHitPolicy = &WarmupHitPolicy;
	}
	else if (MatchState == MatchState::InProgress)
	{
		CurrentHitPolicy = &MatchHitPolicy;
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		CurrentHitPolicy = &PostMatchHitPolicy;
	}
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

	// A death that isn't a player's has nothing to score.
	APawn* Victim = LifecycleComp->GetOwner<APawn>();
	AEnclaveMatchPlayerState* VictimPlayerState = Victim ? Victim->GetPlayerState<AEnclaveMatchPlayerState>() : nullptr;
	if (!IsValid(Victim) || !IsValid(VictimPlayerState))
	{
		return;
	}

	// From the request, not the attacker's pawn: the pawn may have died (and
	// lost its PlayerState) before the damage landed. A player who has left
	// has a destroyed PlayerState, and counts as no attacker.
	APlayerState* SourcePlayerState = DamageRequest.SourcePlayerState.Get();
	AEnclaveMatchPlayerState* AttackerPlayerState = IsValid(SourcePlayerState) ? Cast<AEnclaveMatchPlayerState>(SourcePlayerState) : nullptr;
	// Only an enemy gets the kill: IsHostileTo is false for yourself, teammates and neutral players.
	const bool bKill = AttackerPlayerState && AttackerPlayerState->IsHostileTo(VictimPlayerState);
	if (bKill)
	{
		AttackerPlayerState->AddKill();
	}

	// A team kill (friendly fire) isn't the victim's fault, so it doesn't count
	// as their death. Suicides and deaths with no attacker do.
	const bool bTeamKill = AttackerPlayerState && AttackerPlayerState != VictimPlayerState
		&& AttackerPlayerState->GetAttitudeTowards(VictimPlayerState) == EEnclaveAttitude::Friendly;
	if (!bTeamKill)
	{
		VictimPlayerState->AddDeath();
	}

	// The mode sees every death, so it can also penalize suicides or team kills.
	ScoreKill(AttackerPlayerState, VictimPlayerState);

	// After ScoreKill, so the scores include this death. Player stats only: team
	// scores are the team branch's, and the match-over line reports them.
	const TCHAR* DeathKind = bKill ? TEXT("kill")
		: !AttackerPlayerState ? TEXT("no attacker")
		: AttackerPlayerState == VictimPlayerState ? TEXT("suicide")
		: TEXT("not hostile");
	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState)
	{
		UE_LOG(LogEnclaveMatch, Log, TEXT("Death (%s): %s by %s. Victim deaths %d, score %.0f; attacker kills %d, score %.0f."),
			DeathKind, *VictimPlayerState->GetPlayerName(), *AttackerPlayerState->GetPlayerName(),
			VictimPlayerState->GetDeaths(), VictimPlayerState->GetScore(),
			AttackerPlayerState->GetKills(), AttackerPlayerState->GetScore());
	}
	else
	{
		UE_LOG(LogEnclaveMatch, Log, TEXT("Death (%s): %s. Deaths %d, score %.0f."),
			DeathKind, *VictimPlayerState->GetPlayerName(), VictimPlayerState->GetDeaths(), VictimPlayerState->GetScore());
	}

	AEnclaveMatchGameState* MatchGameState = GetGameState<AEnclaveMatchGameState>();
	if (IsValid(MatchGameState))
	{
		MatchGameState->MulticastKill(AttackerPlayerState, VictimPlayerState);
	}

	// The body stays until the respawn (destroying it here would be mid-damage).
	// One timer per death, so the handle is local: RespawnAfterDeath rejects a
	// player who left or a match that ended, so nothing needs to cancel it.
	// Weak, because the player may disconnect before it fires.
	const TWeakObjectPtr<AController> VictimController = Victim->GetController();
	const FTimerDelegate Respawn = FTimerDelegate::CreateUObject(this, &ThisClass::RespawnAfterDeath, VictimController);

	// A timer with a delay of 0 never fires (FTimerManager::InternalSetTimer
	// only sets one for a rate > 0), so an instant respawn waits for the next
	// tick instead: still outside this damage callback, and through the same
	// checks in RespawnAfterDeath.
	if (MatchSettings.RespawnDelaySeconds > 0)
	{
		FTimerHandle RespawnTimer;
		GetWorldTimerManager().SetTimer(RespawnTimer, Respawn, MatchSettings.RespawnDelaySeconds, false);
	}
	else
	{
		GetWorldTimerManager().SetTimerForNextTick(Respawn);
	}
}

void AEnclaveMatchGameMode::RespawnAfterDeath(TWeakObjectPtr<AController> Controller)
{
	if (!IsMatchInProgress() || !Controller.IsValid())
	{
		return;
	}

	RespawnPlayer(Controller.Get());
}
