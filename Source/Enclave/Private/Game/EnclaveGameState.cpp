// Copyright Lightcarrier Studios


#include "Game/EnclaveGameState.h"
#include "Net/UnrealNetwork.h"

void AEnclaveGameState::InitTeamScores(int32 NumTeams)
{
	if(!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("InitTeamScores called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if (!ensureMsgf(NumTeams >= 0 && NumTeams < AEnclavePlayerState::NoTeam, TEXT("InitTeamScores: invalid team count %d"), NumTeams))
	{
		TeamScores.Reset();   // no teams: scoring visibly stops working instead of quietly limping along
		return;
	}

	TeamScores.Init(0, NumTeams);
}

int32 AEnclaveGameState::GetTeamScore(uint8 TeamId) const
{
	if (TeamScores.IsValidIndex(TeamId))
	{
		return TeamScores[TeamId];
	}

	return 0;
}

void AEnclaveGameState::AddTeamScore(uint8 TeamId, int32 Delta)
{
	if(!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTeamScore called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if(!TeamScores.IsValidIndex(TeamId))
	{
		UE_LOG(LogTemp, Warning, TEXT("AddTeamScore called with invalid TeamId: %d"), TeamId);
		return;
	}

	TeamScores[TeamId] += Delta;
}

void AEnclaveGameState::SetMatchEndServerTime(double ServerTime)
{
	if(!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMatchEndServerTime called on non-authority GameState. This should only be called on the server."));
		return;
	}

	MatchEndServerTime = ServerTime;
}

void AEnclaveGameState::SetWinner(uint8 TeamId, APlayerState* Player)
{
	if(!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SetWinner called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if(!ensureMsgf(TeamId == AEnclavePlayerState::NoTeam || TeamScores.IsValidIndex(TeamId), TEXT("SetWinner: invalid TeamId %d"), TeamId))
	{
		return;
	}

	WinningTeam = TeamId;
	WinningPlayer = Player;
}

void AEnclaveGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveGameState, TeamScores);
	DOREPLIFETIME(AEnclaveGameState, MatchEndServerTime);
	DOREPLIFETIME(AEnclaveGameState, WinningTeam);
	DOREPLIFETIME(AEnclaveGameState, WinningPlayer);
}

void AEnclaveGameState::MulticastKill_Implementation(APlayerState* Killer, APlayerState* Victim)
{
	OnKill.Broadcast(Killer, Victim);
}