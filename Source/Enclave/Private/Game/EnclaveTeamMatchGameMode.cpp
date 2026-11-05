// Copyright Lightcarrier Studios

#include "Game/EnclaveTeamMatchGameMode.h"

#include "GameFramework/PlayerController.h"
#include "Math/RandomStream.h"

#include "Debug/EnclaveDebug.h"
#include "Game/EnclaveTeamGameState.h"
#include "Player/EnclaveTeamPlayerState.h"

AEnclaveTeamMatchGameMode::AEnclaveTeamMatchGameMode()
{
	GameStateClass = AEnclaveTeamGameState::StaticClass();
	PlayerStateClass = AEnclaveTeamPlayerState::StaticClass();
}

void AEnclaveTeamMatchGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	TeamAssignmentRules.ApplyOptions(Options);
	UE_LOG(LogEnclaveMatch, Log, TEXT("Team settings: NumTeams=%d %s"), NumTeams, *TeamAssignmentRules.ToString());
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

	// Also before Super, which spawns everyone: spawn selection needs the teams.
	AssignTeamsAtMatchStart();

	Super::HandleMatchHasStarted();
}

void AEnclaveTeamMatchGameMode::AssignTeamsAtMatchStart()
{
	if (!GameState)
	{
		return;
	}

	// One group per connected player; parties will make larger groups. Players
	// who left aren't assigned: on reconnect, HandleStartingNewPlayer places them.
	TArray<AEnclaveTeamPlayerState*> Players;
	TArray<FEnclaveTeamGroup> Groups;
	for (APlayerState* Player : GameState->PlayerArray)
	{
		AEnclaveTeamPlayerState* TeamPlayer = Cast<AEnclaveTeamPlayerState>(Player);
		if (!TeamPlayer || TeamPlayer->IsOnlyASpectator())
		{
			continue;
		}

		FEnclaveTeamGroup& Group = Groups.AddDefaulted_GetRef();
		Group.PreviousTeam = TeamPlayer->GetTeamId();
		Group.JoinOrder = TeamPlayer->GetPlayerId();   // assigned in connection order; kept by seamless travel
		Players.Add(TeamPlayer);
	}

	// A fresh seed per match, logged so a surprising result can be replayed in a test.
	FRandomStream Random;
	Random.GenerateNewSeed();
	const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, TeamAssignmentRules, NumTeams, Random);

	int32 NumChanged = 0;
	TArray<FString> TeamRosters;
	TeamRosters.SetNum(NumTeams);
	for (int32 Index = 0; Index < Players.Num(); ++Index)
	{
		if (Players[Index]->GetTeamId() != Teams[Index])
		{
			++NumChanged;
		}
		Players[Index]->SetTeamId(Teams[Index]);
		if (Teams[Index] < NumTeams)
		{
			FString& Roster = TeamRosters[Teams[Index]];
			Roster += (Roster.IsEmpty() ? TEXT("") : TEXT(", ")) + Players[Index]->GetPlayerName();
		}
	}

	FString Summary;
	for (int32 TeamId = 0; TeamId < NumTeams; ++TeamId)
	{
		Summary += FString::Printf(TEXT("%steam %d: %s"), Summary.IsEmpty() ? TEXT("") : TEXT("; "),
			TeamId, TeamRosters[TeamId].IsEmpty() ? TEXT("(empty)") : *TeamRosters[TeamId]);
	}
	UE_LOG(LogEnclaveMatch, Log, TEXT("Teams at match start (%s, seed %d, %d of %d changed team): %s."),
		*TeamAssignmentRules.ToString(), Random.GetInitialSeed(), NumChanged, Players.Num(), *Summary);
}

bool AEnclaveTeamMatchGameMode::FindInactivePlayer(APlayerController* PC)
{
	const bool bFound = Super::FindInactivePlayer(PC);
	if (bFound)
	{
		ReconnectingPlayer = PC;
	}
	return bFound;
}

void AEnclaveTeamMatchGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Read and clear the reconnect note first, whoever arrives, so it can never
	// apply to a later player.
	const bool bReconnect = NewPlayer && ReconnectingPlayer.Get() == NewPlayer;
	ReconnectingPlayer.Reset();

	AEnclaveTeamPlayerState* PlayerState = NewPlayer ? NewPlayer->GetPlayerState<AEnclaveTeamPlayerState>() : nullptr;

	// Spectators stay on NoTeam.
	if (PlayerState && !MustSpectate(NewPlayer))
	{
		// An out-of-range team (from a mode with more teams) counts as none.
		const uint8 CurrentTeam = PlayerState->GetTeamId();
		const bool bHasValidTeam = CurrentTeam < NumTeams;

		// Before the match starts, any valid team is fine: match start reassigns.
		// After, only a reconnect may keep its team, and only if that keeps the
		// sizes within the limit (balanced, or their old team is short). Anyone
		// else's team was decided before this match's assignment: place them anew.
		bool bKeepTeam = false;
		if (bHasValidTeam && !IsMatchInProgress())
		{
			bKeepTeam = true;
		}
		else if (bHasValidTeam && bReconnect)
		{
			int32 SmallestCount = MAX_int32;
			for (int32 TeamId = 0; TeamId < NumTeams; ++TeamId)
			{
				SmallestCount = FMath::Min(SmallestCount, CountPlayersOnTeam(static_cast<uint8>(TeamId), PlayerState));
			}
			const int32 OldTeamCountWithPlayer = CountPlayersOnTeam(CurrentTeam, PlayerState) + 1;
			bKeepTeam = OldTeamCountWithPlayer - SmallestCount <= FMath::Max(1, TeamAssignmentRules.MaxTeamSizeDifference);
		}

		if (bKeepTeam)
		{
			UE_LOG(LogEnclaveMatch, Log, TEXT("%s keeps team %d (%s)."), *PlayerState->GetPlayerName(), CurrentTeam,
				bReconnect ? TEXT("reconnect") : TEXT("match not started"));
		}
		else
		{
			const uint8 NewTeam = ChooseTeamForPlayer(NewPlayer);
			PlayerState->SetTeamId(NewTeam);
			const FString Reason = !bHasValidTeam ? FString(TEXT("new player"))
				: bReconnect ? FString::Printf(TEXT("reconnect; team %d is full"), CurrentTeam)
				: FString::Printf(TEXT("arrived after the start with team %d"), CurrentTeam);
			UE_LOG(LogEnclaveMatch, Log, TEXT("%s joins team %d (%s; %d players on it now)."),
				*PlayerState->GetPlayerName(), NewTeam, *Reason, CountPlayersOnTeam(NewTeam));
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
	// The player may still carry an old team; it mustn't count toward that team.
	const APlayerState* PlayerState = Player ? Player->PlayerState.Get() : nullptr;

	// The only policy so far is Random: the smallest team, ties at random.
	// SkillBalance would break ties by rating.
	TArray<uint8, TInlineAllocator<8>> SmallestTeams;
	int32 SmallestCount = MAX_int32;
	for (int32 TeamId = 0; TeamId < NumTeams; ++TeamId)
	{
		const int32 Count = CountPlayersOnTeam(static_cast<uint8>(TeamId), PlayerState);
		if (Count < SmallestCount)
		{
			SmallestTeams.Reset();
			SmallestCount = Count;
		}
		if (Count == SmallestCount)
		{
			SmallestTeams.Add(static_cast<uint8>(TeamId));
		}
	}

	return SmallestTeams.IsEmpty() ? 0 : SmallestTeams[FMath::RandRange(0, SmallestTeams.Num() - 1)];
}

int32 AEnclaveTeamMatchGameMode::CountPlayersOnTeam(uint8 TeamId, const APlayerState* ExcludedPlayer) const
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
		if (TeamPlayer && TeamPlayer != ExcludedPlayer && !TeamPlayer->IsOnlyASpectator() && TeamPlayer->GetTeamId() == TeamId)
		{
			++Count;
		}
	}

	return Count;
}
