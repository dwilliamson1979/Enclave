// Copyright Lightcarrier Studios

#include "Game/EnclaveTeamMatchGameMode.h"

#include "GameFramework/PlayerController.h"

#include "Debug/EnclaveDebug.h"
#include "Game/EnclaveTeamGameState.h"
#include "Player/EnclaveTeamPlayerState.h"

AEnclaveTeamMatchGameMode::AEnclaveTeamMatchGameMode()
{
	GameStateClass = AEnclaveTeamGameState::StaticClass();
	PlayerStateClass = AEnclaveTeamPlayerState::StaticClass();
}

void AEnclaveTeamMatchGameMode::InitGameState()
{
	Super::InitGameState();

	// Players get their team on joining, during warmup, so the GameState
	// should already describe the teams then. Match start resets them again.
	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();
	if (ensureMsgf(TeamGameState, TEXT("AEnclaveTeamMatchGameMode needs an AEnclaveTeamGameState; teams were not set up.")))
	{
		TeamGameState->InitTeams(NumTeams);
	}
}

void AEnclaveTeamMatchGameMode::HandleMatchHasStarted()
{
	// Before Super: Super ends with the engine spawning every player, and they
	// must spawn into a match whose team state is already set up.
	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();
	if (ensureMsgf(TeamGameState, TEXT("AEnclaveTeamMatchGameMode needs an AEnclaveTeamGameState; teams were not set up.")))
	{
		TeamGameState->InitTeams(NumTeams);
	}

	Super::HandleMatchHasStarted();
}

void AEnclaveTeamMatchGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	AEnclaveTeamPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AEnclaveTeamPlayerState>() : nullptr;

	// Spectators stay on NoTeam.
	if (PlayerState && !MustSpectate(NewPlayer))
	{
		// A valid team means a reconnect (or seamless travel) restored it: keep it.
		// An out-of-range team can only come from a mode with more teams: replace it.
		const uint8 CurrentTeam = PlayerState->GetTeamId();
		if (CurrentTeam != AEnclaveTeamPlayerState::NoTeam && CurrentTeam < NumTeams)
		{
			UE_LOG(LogEnclaveMatch, Log, TEXT("%s keeps team %d."), *PlayerState->GetPlayerName(), CurrentTeam);
		}
		else
		{
			const uint8 NewTeam = ChooseTeamForPlayer(NewPlayer);
			PlayerState->SetTeamId(NewTeam);
			UE_LOG(LogEnclaveMatch, Log, TEXT("%s joins team %d (%d players on it now)."),
				*PlayerState->GetPlayerName(), NewTeam, CountPlayersOnTeam(NewTeam));
		}
	}

	// Last: if the match is in progress, Super spawns the pawn, and spawn
	// selection needs the team.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

uint8 AEnclaveTeamMatchGameMode::ChooseTeamForPlayer(const AController* Player) const
{
	uint8 SmallestTeam = 0;
	int32 SmallestCount = MAX_int32;

	for (int32 TeamId = 0; TeamId < NumTeams; ++TeamId)
	{
		const int32 Count = CountPlayersOnTeam(static_cast<uint8>(TeamId));
		if (Count < SmallestCount)   // strict: a tie keeps the lower team ID
		{
			SmallestTeam = static_cast<uint8>(TeamId);
			SmallestCount = Count;
		}
	}

	return SmallestTeam;
}

int32 AEnclaveTeamMatchGameMode::CountPlayersOnTeam(uint8 TeamId) const
{
	if (!GameState)
	{
		return 0;
	}

	// PlayerArray holds connected players only; players who left are in
	// InactivePlayerArray, so they don't hold a place on a team.
	int32 Count = 0;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		const AEnclaveTeamPlayerState* TeamPlayer = Cast<AEnclaveTeamPlayerState>(Player);
		if (TeamPlayer && !TeamPlayer->IsOnlyASpectator() && TeamPlayer->GetTeamId() == TeamId)
		{
			++Count;
		}
	}

	return Count;
}
