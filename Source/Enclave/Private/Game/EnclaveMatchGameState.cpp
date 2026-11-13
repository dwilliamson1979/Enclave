// Copyright Lightcarrier Studios

#include "Game/EnclaveMatchGameState.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveMatchGameState::SetPhaseEndServerTime(double ServerTime)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("SetPhaseEndServerTime called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if (PhaseEndServerTime != ServerTime)
	{
		PhaseEndServerTime = ServerTime;
		// The server gets no OnRep; call it so the listen host's HUD hears it.
		OnRep_PhaseEndServerTime();
	}
}

void AEnclaveMatchGameState::SetScoreLimit(int32 NewScoreLimit)
{
	if (!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("SetScoreLimit called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if (ScoreLimit != NewScoreLimit)
	{
		ScoreLimit = NewScoreLimit;
		OnRep_ScoreLimit();
	}
}

void AEnclaveMatchGameState::OnRep_ScoreLimit()
{
	OnScoreLimitChanged.Broadcast();
}

void AEnclaveMatchGameState::NotifyPlayerScoreChanged(APlayerState* PlayerState)
{
	OnPlayerScoreChanged.Broadcast(PlayerState);
}

void AEnclaveMatchGameState::AddPlayerState(APlayerState* PlayerState)
{
	Super::AddPlayerState(PlayerState);

	OnPlayersChanged.Broadcast();
}

void AEnclaveMatchGameState::RemovePlayerState(APlayerState* PlayerState)
{
	Super::RemovePlayerState(PlayerState);

	OnPlayersChanged.Broadcast();
}

void AEnclaveMatchGameState::OnRep_MatchState()
{
	Super::OnRep_MatchState();

	OnPhaseChanged.Broadcast();
}

void AEnclaveMatchGameState::OnRep_PhaseEndServerTime()
{
	OnPhaseChanged.Broadcast();
}

void AEnclaveMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveMatchGameState, PhaseEndServerTime);
	DOREPLIFETIME(AEnclaveMatchGameState, ScoreLimit);
}

void AEnclaveMatchGameState::MulticastKill_Implementation(APlayerState* Killer, APlayerState* Victim)
{
	OnKill.Broadcast(Killer, Victim);
}
