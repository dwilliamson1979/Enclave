// Copyright Lightcarrier Studios

#include "UI/EnclaveMatchTimerWidget.h"

#include "Components/TextBlock.h"
#include "Game/EnclaveMatchGameState.h"
#include "GameFramework/GameMode.h"

void UEnclaveMatchTimerWidget::NativeDestruct()
{
	if (AEnclaveMatchGameState* GameState = MatchGameState.Get())
	{
		GameState->OnPhaseChanged.RemoveDynamic(this, &UEnclaveMatchTimerWidget::HandlePhaseChanged);
	}
	MatchGameState.Reset();

	Super::NativeDestruct();
}

void UEnclaveMatchTimerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshTime();
}

void UEnclaveMatchTimerWidget::OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState)
{
	if (AEnclaveMatchGameState* OldMatchGameState = Cast<AEnclaveMatchGameState>(OldGameState))
	{
		OldMatchGameState->OnPhaseChanged.RemoveDynamic(this, &UEnclaveMatchTimerWidget::HandlePhaseChanged);
	}

	MatchGameState = Cast<AEnclaveMatchGameState>(NewGameState);
	if (AEnclaveMatchGameState* NewMatchGameState = MatchGameState.Get())
	{
		NewMatchGameState->OnPhaseChanged.AddUniqueDynamic(this, &UEnclaveMatchTimerWidget::HandlePhaseChanged);
	}

	// Show what is already there: a late joiner gets no change event for it.
	HandlePhaseChanged();

	Super::OnGameStateChanged(OldGameState, NewGameState);
}

void UEnclaveMatchTimerWidget::HandlePhaseChanged()
{
	const AEnclaveMatchGameState* GameState = MatchGameState.Get();
	const FName State = GameState ? GameState->GetMatchState() : NAME_None;
	const bool bHasEnd = GameState && GameState->GetPhaseEndServerTime() > 0.0;

	FText Label;
	if (State == MatchState::WaitingToStart)
	{
		Label = bHasEnd ? WarmupLabel : WaitingForPlayersLabel;
	}
	else if (State == MatchState::InProgress)
	{
		Label = InProgressLabel;
	}
	else if (State == MatchState::WaitingPostMatch)
	{
		Label = PostMatchLabel;
	}

	if (PhaseText)
	{
		PhaseText->SetText(Label);
		PhaseText->SetVisibility(Label.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}

	// The end may have moved (a new phase, or a countdown that started): show it now.
	ShownSeconds = INDEX_NONE;
	RefreshTime();
}

void UEnclaveMatchTimerWidget::RefreshTime()
{
	if (!TimeText)
	{
		return;
	}

	const AEnclaveMatchGameState* GameState = MatchGameState.Get();
	const double PhaseEnd = GameState ? GameState->GetPhaseEndServerTime() : 0.0;

	// No end (no GameState yet, waiting for players, no time limit, or a
	// phase such as LeavingMap with no countdown): hide the time.
	const FName State = GameState ? GameState->GetMatchState() : NAME_None;
	const bool bCountsDown = PhaseEnd > 0.0 && (State == MatchState::WaitingToStart
		|| State == MatchState::InProgress || State == MatchState::WaitingPostMatch);
	if (!bCountsDown)
	{
		if (ShownSeconds != INDEX_NONE || TimeText->GetVisibility() != ESlateVisibility::Collapsed)
		{
			ShownSeconds = INDEX_NONE;
			TimeText->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// Round up, so "0:01" shows for the last second and "0:00" only at the end.
	const int32 Seconds = FMath::Max(0, FMath::CeilToInt32(PhaseEnd - GameState->GetServerWorldTimeSeconds()));
	if (Seconds == ShownSeconds)
	{
		return;
	}

	ShownSeconds = Seconds;
	TimeText->SetText(FText::FromString(FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60)));
	TimeText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
