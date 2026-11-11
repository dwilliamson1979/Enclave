// Copyright Lightcarrier Studios

#include "Game/EnclaveSoloMatchGameMode.h"

#include "GameFramework/PlayerState.h"

#include "Debug/EnclaveDebug.h"
#include "Game/EnclaveSoloGameState.h"

AEnclaveSoloMatchGameMode::AEnclaveSoloMatchGameMode()
{
	GameStateClass = AEnclaveSoloGameState::StaticClass();
}

void AEnclaveSoloMatchGameMode::HandleMatchHasStarted()
{
	// Before Super: Super ends with the engine spawning every player, and they
	// must spawn into a match with no result yet.
	AEnclaveSoloGameState* SoloGameState = GetGameState<AEnclaveSoloGameState>();
	if (ensureMsgf(SoloGameState, TEXT("AEnclaveSoloMatchGameMode needs an AEnclaveSoloGameState; the result was not cleared.")))
	{
		SoloGameState->SetWinningPlayer(nullptr);
	}

	Super::HandleMatchHasStarted();
}

bool AEnclaveSoloMatchGameMode::CheckWinCondition(bool bTimeLimitReached)
{
	AEnclaveSoloGameState* SoloGameState = GetGameState<AEnclaveSoloGameState>();
	if (!ensureMsgf(SoloGameState, TEXT("AEnclaveSoloMatchGameMode needs an AEnclaveSoloGameState; the match can't end.")))
	{
		return false;
	}

	// The top score, and how many players share it. PlayerArray holds connected
	// players only: a leaver's PlayerState is destroyed, which removes it
	// (PlayerState.cpp:204-209), and the copy kept for a reconnect is taken out
	// (GameMode.cpp:608-612). Score is the replicated APlayerState::Score, the
	// value players see; whole numbers are exact in a float.
	float TopScore = 0.0f;
	APlayerState* TopPlayer = nullptr;
	int32 NumPlayersAtTop = 0;
	for (APlayerState* Player : SoloGameState->PlayerArray)
	{
		// A spectator takes no part, so they can't win or tie the leader.
		if (!Player || Player->IsOnlyASpectator())
		{
			continue;
		}

		const float Score = Player->GetScore();
		if (NumPlayersAtTop == 0 || Score > TopScore)
		{
			TopScore = Score;
			TopPlayer = Player;
			NumPlayersAtTop = 1;
		}
		else if (Score == TopScore)
		{
			++NumPlayersAtTop;
		}
	}

	// >=, not ==: a mode may score more than 1 at a time, and kills of the same
	// frame are all scored before this poll.
	const bool bScoreLimitReached = MatchSettings.ScoreLimit > 0
		&& NumPlayersAtTop > 0
		&& TopScore >= static_cast<float>(MatchSettings.ScoreLimit);
	if (!bScoreLimitReached && !bTimeLimitReached)
	{
		return false;
	}

	// A shared top score is a draw, at either end: the order of kills within a
	// frame is packet order. No players at all is also a draw. A lone player
	// wins, even on 0.
	APlayerState* const WinningPlayer = NumPlayersAtTop == 1 ? TopPlayer : nullptr;
	SoloGameState->SetWinningPlayer(WinningPlayer);

	// Runs once: returning true ends the match, so polling stops.
	const FString Result = WinningPlayer
		? FString::Printf(TEXT("%s wins with %d"), *WinningPlayer->GetPlayerName(), FMath::RoundToInt(TopScore))
		: FString::Printf(TEXT("a draw, %d player(s) on %d"), NumPlayersAtTop, FMath::RoundToInt(TopScore));
	UE_LOG(LogEnclaveMatch, Log, TEXT("Match over (%s): %s."),
		bScoreLimitReached ? TEXT("score limit") : TEXT("time limit"), *Result);

	return true;
}
