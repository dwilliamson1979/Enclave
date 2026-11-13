// Copyright Lightcarrier Studios

#include "UI/EnclaveScoreLimitWidget.h"

#include "Components/TextBlock.h"
#include "Game/EnclaveMatchGameState.h"

void UEnclaveScoreLimitWidget::NativeDestruct()
{
	if (AEnclaveMatchGameState* GameState = MatchGameState.Get())
	{
		GameState->OnScoreLimitChanged.RemoveDynamic(this, &UEnclaveScoreLimitWidget::HandleScoreLimitChanged);
	}
	MatchGameState.Reset();

	Super::NativeDestruct();
}

void UEnclaveScoreLimitWidget::OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState)
{
	if (AEnclaveMatchGameState* OldMatchGameState = Cast<AEnclaveMatchGameState>(OldGameState))
	{
		OldMatchGameState->OnScoreLimitChanged.RemoveDynamic(this, &UEnclaveScoreLimitWidget::HandleScoreLimitChanged);
	}

	MatchGameState = Cast<AEnclaveMatchGameState>(NewGameState);
	if (AEnclaveMatchGameState* NewMatchGameState = MatchGameState.Get())
	{
		NewMatchGameState->OnScoreLimitChanged.AddUniqueDynamic(this, &UEnclaveScoreLimitWidget::HandleScoreLimitChanged);
	}

	// Show what is already there; the change event only covers what follows.
	HandleScoreLimitChanged();

	Super::OnGameStateChanged(OldGameState, NewGameState);
}

void UEnclaveScoreLimitWidget::HandleScoreLimitChanged()
{
	if (!LimitText)
	{
		return;
	}

	const AEnclaveMatchGameState* GameState = MatchGameState.Get();
	const int32 ScoreLimit = GameState ? GameState->GetScoreLimit() : 0;
	if (ScoreLimit > 0)
	{
		LimitText->SetText(FText::Format(LimitFormat, FText::AsNumber(ScoreLimit)));
		LimitText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		LimitText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
