// Copyright Lightcarrier Studios

#include "UI/EnclaveTeamScoreWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Game/EnclaveTeamGameState.h"
#include "Player/EnclaveTeamPlayerState.h"

// ---------------------------------------------------------------------------
// Entry

void UEnclaveTeamScoreEntryWidget::SetTeam(uint8 InTeamId, bool bInIsLocalTeam)
{
	if (TeamId == InTeamId && bIsLocalTeam == bInIsLocalTeam)
	{
		return;
	}

	TeamId = InTeamId;
	bIsLocalTeam = bInIsLocalTeam;

	if (TeamNameText)
	{
		TeamNameText->SetText(FText::Format(TeamNameFormat, FText::AsNumber(static_cast<int32>(TeamId) + 1)));
	}
	OnTeamSet(TeamId, bIsLocalTeam);
}

void UEnclaveTeamScoreEntryWidget::SetScore(int32 Score)
{
	if (ScoreText && Score != ShownScore)
	{
		ShownScore = Score;
		ScoreText->SetText(FText::AsNumber(Score));
	}
}

// ---------------------------------------------------------------------------
// Team scores

void UEnclaveTeamScoreWidget::NativeDestruct()
{
	UnbindGameState();
	UnbindPlayerState();

	Super::NativeDestruct();
}

void UEnclaveTeamScoreWidget::OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState)
{
	UnbindGameState();
	BindGameState(Cast<AEnclaveTeamGameState>(NewGameState));

	// Show what is already there; the change events only cover what follows.
	RefreshScores();

	Super::OnGameStateChanged(OldGameState, NewGameState);
}

void UEnclaveTeamScoreWidget::OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState)
{
	UnbindPlayerState();
	BindPlayerState(Cast<AEnclaveTeamPlayerState>(NewPlayerState));

	RefreshLocalTeam();

	Super::OnPlayerStateChanged(OldPlayerState, NewPlayerState);
}

void UEnclaveTeamScoreWidget::BindGameState(AEnclaveTeamGameState* GameState)
{
	TeamGameState = GameState;
	if (GameState)
	{
		GameState->OnTeamScoresChanged.AddUniqueDynamic(this, &UEnclaveTeamScoreWidget::HandleTeamScoresChanged);
	}
}

void UEnclaveTeamScoreWidget::UnbindGameState()
{
	if (AEnclaveTeamGameState* GameState = TeamGameState.Get())
	{
		GameState->OnTeamScoresChanged.RemoveDynamic(this, &UEnclaveTeamScoreWidget::HandleTeamScoresChanged);
	}
	TeamGameState.Reset();
}

void UEnclaveTeamScoreWidget::BindPlayerState(AEnclaveTeamPlayerState* PlayerState)
{
	LocalPlayerState = PlayerState;
	if (PlayerState)
	{
		PlayerState->OnTeamChanged.AddUniqueDynamic(this, &UEnclaveTeamScoreWidget::HandleTeamChanged);
	}
}

void UEnclaveTeamScoreWidget::UnbindPlayerState()
{
	if (AEnclaveTeamPlayerState* PlayerState = LocalPlayerState.Get())
	{
		PlayerState->OnTeamChanged.RemoveDynamic(this, &UEnclaveTeamScoreWidget::HandleTeamChanged);
	}
	LocalPlayerState.Reset();
}

void UEnclaveTeamScoreWidget::HandleTeamScoresChanged()
{
	RefreshScores();
}

void UEnclaveTeamScoreWidget::HandleTeamChanged(AEnclaveTeamPlayerState* PlayerState, uint8 NewTeamId)
{
	RefreshLocalTeam();
}

void UEnclaveTeamScoreWidget::RefreshScores()
{
	const AEnclaveTeamGameState* GameState = TeamGameState.Get();
	const int32 NumTeams = GameState ? GameState->GetNumTeams() : 0;

	// The team count is fixed for a match, so entries are rebuilt only when it
	// first arrives (or a mode with a different count is shown).
	if (Entries.Num() != NumTeams && TeamList && EntryClass)
	{
		TeamList->ClearChildren();
		Entries.Reset();

		const uint8 LocalTeamId = GetLocalTeamId();
		for (int32 Index = 0; Index < NumTeams; ++Index)
		{
			UEnclaveTeamScoreEntryWidget* Entry = CreateWidget<UEnclaveTeamScoreEntryWidget>(this, EntryClass);
			if (!Entry)
			{
				continue;
			}
			const uint8 EntryTeamId = static_cast<uint8>(Index);
			Entry->SetTeam(EntryTeamId, EntryTeamId == LocalTeamId);
			TeamList->AddChild(Entry);
			Entries.Add(Entry);
		}
	}

	for (UEnclaveTeamScoreEntryWidget* Entry : Entries)
	{
		if (Entry && GameState)
		{
			Entry->SetScore(GameState->GetTeamScore(Entry->GetTeamId()));
		}
	}
}

void UEnclaveTeamScoreWidget::RefreshLocalTeam()
{
	const uint8 LocalTeamId = GetLocalTeamId();
	for (UEnclaveTeamScoreEntryWidget* Entry : Entries)
	{
		if (Entry)
		{
			Entry->SetTeam(Entry->GetTeamId(), Entry->GetTeamId() == LocalTeamId);
		}
	}
}

uint8 UEnclaveTeamScoreWidget::GetLocalTeamId() const
{
	const AEnclaveTeamPlayerState* PlayerState = LocalPlayerState.Get();
	return PlayerState ? PlayerState->GetTeamId() : AEnclaveTeamPlayerState::NoTeam;
}
