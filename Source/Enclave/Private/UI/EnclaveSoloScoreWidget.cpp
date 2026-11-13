// Copyright Lightcarrier Studios

#include "UI/EnclaveSoloScoreWidget.h"

#include "Components/TextBlock.h"
#include "Game/EnclaveMatchGameState.h"
#include "GameFramework/PlayerState.h"

void UEnclaveSoloScoreWidget::NativeDestruct()
{
	UnbindGameState();

	Super::NativeDestruct();
}

void UEnclaveSoloScoreWidget::OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState)
{
	UnbindGameState();
	BindGameState(Cast<AEnclaveMatchGameState>(NewGameState));

	// Show what is already there; the change events only cover what follows.
	RefreshScores();

	Super::OnGameStateChanged(OldGameState, NewGameState);
}

void UEnclaveSoloScoreWidget::OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState)
{
	// No binding of its own: the GameState reports every player's score.
	LocalPlayerState = NewPlayerState;
	RefreshScores();

	Super::OnPlayerStateChanged(OldPlayerState, NewPlayerState);
}

void UEnclaveSoloScoreWidget::BindGameState(AEnclaveMatchGameState* GameState)
{
	MatchGameState = GameState;
	if (GameState)
	{
		GameState->OnPlayerScoreChanged.AddUniqueDynamic(this, &UEnclaveSoloScoreWidget::HandlePlayerScoreChanged);
		GameState->OnPlayersChanged.AddUniqueDynamic(this, &UEnclaveSoloScoreWidget::HandlePlayersChanged);
	}
}

void UEnclaveSoloScoreWidget::UnbindGameState()
{
	if (AEnclaveMatchGameState* GameState = MatchGameState.Get())
	{
		GameState->OnPlayerScoreChanged.RemoveDynamic(this, &UEnclaveSoloScoreWidget::HandlePlayerScoreChanged);
		GameState->OnPlayersChanged.RemoveDynamic(this, &UEnclaveSoloScoreWidget::HandlePlayersChanged);
	}
	MatchGameState.Reset();
}

void UEnclaveSoloScoreWidget::HandlePlayerScoreChanged(APlayerState* PlayerState)
{
	RefreshScores();
}

void UEnclaveSoloScoreWidget::HandlePlayersChanged()
{
	RefreshScores();
}

void UEnclaveSoloScoreWidget::RefreshScores()
{
	const AEnclaveMatchGameState* GameState = MatchGameState.Get();
	const APlayerState* Local = LocalPlayerState.Get();
	const bool bLocalPlays = Local && !Local->IsOnlyASpectator();
	const float LocalScore = bLocalPlays ? Local->GetScore() : 0.0f;

	// The rival: the best score among the other players. A full scan, but only
	// on a score change or a join or leave, over a match's few players.
	bool bHasRival = false;
	float RivalScore = 0.0f;
	if (GameState)
	{
		for (const APlayerState* Player : GameState->PlayerArray)
		{
			if (!Player || Player == Local || Player->IsOnlyASpectator())
			{
				continue;
			}
			if (!bHasRival || Player->GetScore() > RivalScore)
			{
				RivalScore = Player->GetScore();
				bHasRival = true;
			}
		}
	}

	// Leading means strictly ahead: a tie shows the other player as the leader.
	const bool bIsLeading = bLocalPlays && (!bHasRival || LocalScore > RivalScore);

	ShowScore(PlayerScoreText, LocalScore, bLocalPlays);
	ShowScore(RivalScoreText, RivalScore, bHasRival);
	if (RivalLabelText)
	{
		RivalLabelText->SetText(bIsLeading ? SecondLabel : LeaderLabel);
		RivalLabelText->SetVisibility(bHasRival ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (!bWasLeading.IsSet() || bWasLeading.GetValue() != bIsLeading)
	{
		bWasLeading = bIsLeading;
		OnLeadChanged(bIsLeading);
	}
}

void UEnclaveSoloScoreWidget::ShowScore(UTextBlock* Text, float Score, bool bVisible)
{
	if (!Text)
	{
		return;
	}

	if (bVisible)
	{
		// Scores are whole numbers held in the engine's float Score.
		Text->SetText(FText::AsNumber(FMath::RoundToInt32(Score)));
		Text->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		Text->SetVisibility(ESlateVisibility::Collapsed);
	}
}
