// Copyright Lightcarrier Studios

#include "Game/EnclaveTeamGameState.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveTeamGameState::InitTeams(int32 NumTeams)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("InitTeams called on non-authority GameState. This should only be called on the server."));
		return;
	}

	WinningTeam = AEnclaveTeamPlayerState::NoTeam;

	if (!ensureMsgf(NumTeams > 0 && NumTeams < AEnclaveTeamPlayerState::NoTeam, TEXT("InitTeams: invalid team count %d"), NumTeams))
	{
		TeamScores.Reset();   // no teams: scoring visibly stops working instead of quietly limping along
		return;
	}

	TeamScores.Init(0, NumTeams);
}

int32 AEnclaveTeamGameState::GetTeamScore(uint8 TeamId) const
{
	if (TeamScores.IsValidIndex(TeamId))
	{
		return TeamScores[TeamId];
	}

	return 0;
}

void AEnclaveTeamGameState::AddTeamScore(uint8 TeamId, int32 Delta)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddTeamScore called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if(!TeamScores.IsValidIndex(TeamId))
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddTeamScore called with invalid TeamId: %d"), TeamId);
		return;
	}

	TeamScores[TeamId] += Delta;
}

void AEnclaveTeamGameState::SetWinningTeam(uint8 TeamId)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("SetWinningTeam called on non-authority GameState. This should only be called on the server."));
		return;
	}

	if(!ensureMsgf(TeamId == AEnclaveTeamPlayerState::NoTeam || TeamScores.IsValidIndex(TeamId), TEXT("SetWinningTeam: invalid TeamId %d"), TeamId))
	{
		return;
	}

	WinningTeam = TeamId;
}

void AEnclaveTeamGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveTeamGameState, TeamScores);
	DOREPLIFETIME(AEnclaveTeamGameState, WinningTeam);
}
