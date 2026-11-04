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

bool AEnclaveTeamMatchGameMode::CheckWinCondition(bool bTimeLimitReached)
{
	AEnclaveTeamGameState* TeamGameState = GetGameState<AEnclaveTeamGameState>();
	if (!ensureMsgf(TeamGameState, TEXT("AEnclaveTeamMatchGameMode needs an AEnclaveTeamGameState; the match can't end.")))
	{
		return false;
	}

	// The top score, and how many teams share it. The GameState's scores are the
	// ones players see, so the result can't disagree with the HUD.
	int32 TopScore = 0;
	uint8 TopTeam = AEnclaveTeamPlayerState::NoTeam;
	int32 NumTeamsAtTop = 0;
	for (int32 TeamId = 0; TeamId < TeamGameState->GetNumTeams(); ++TeamId)
	{
		const int32 Score = TeamGameState->GetTeamScore(static_cast<uint8>(TeamId));
		if (NumTeamsAtTop == 0 || Score > TopScore)
		{
			TopScore = Score;
			TopTeam = static_cast<uint8>(TeamId);
			NumTeamsAtTop = 1;
		}
		else if (Score == TopScore)
		{
			++NumTeamsAtTop;
		}
	}

	// >=, not ==: a mode may score more than 1 at a time, and kills can still
	// land in the frame after the limit is reached (see below).
	const bool bScoreLimitReached = MatchSettings.ScoreLimit > 0
		&& NumTeamsAtTop > 0
		&& TopScore >= MatchSettings.ScoreLimit;
	if (!bScoreLimitReached && !bTimeLimitReached)
	{
		return false;
	}

	// One rule for both ends. The world tick receives network traffic before
	// actors tick, so every kill of this frame is already scored and two teams
	// can reach the limit together. Their order would be packet order, which
	// isn't fair to anyone, so a shared top score is a draw. No teams at all
	// (InitTeams failed) is also a draw, so the time limit still ends the match.
	const uint8 WinningTeam = NumTeamsAtTop == 1 ? TopTeam : AEnclaveTeamPlayerState::NoTeam;
	TeamGameState->SetWinningTeam(WinningTeam);

	// Runs once: returning true ends the match, so polling stops.
	FString Scores;
	for (int32 TeamId = 0; TeamId < TeamGameState->GetNumTeams(); ++TeamId)
	{
		Scores += FString::Printf(TEXT("%steam %d: %d"), Scores.IsEmpty() ? TEXT("") : TEXT(", "),
			TeamId, TeamGameState->GetTeamScore(static_cast<uint8>(TeamId)));
	}
	const FString Result = WinningTeam == AEnclaveTeamPlayerState::NoTeam
		? FString(TEXT("a draw"))
		: FString::Printf(TEXT("team %d wins"), WinningTeam);
	UE_LOG(LogEnclaveMatch, Log, TEXT("Match over (%s): %s (%s)."),
		bScoreLimitReached ? TEXT("score limit") : TEXT("time limit"), *Result, *Scores);

	return true;
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
