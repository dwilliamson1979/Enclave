// Copyright Lightcarrier Studios

#include "UI/EnclaveKillFeedWidget.h"

#include "Components/PanelWidget.h"
#include "Game/EnclaveMatchGameState.h"
#include "UI/EnclaveKillFeedEntryWidget.h"

void UEnclaveKillFeedWidget::NativeDestruct()
{
	if (AEnclaveMatchGameState* GameState = MatchGameState.Get())
	{
		GameState->OnKill.RemoveDynamic(this, &UEnclaveKillFeedWidget::HandleKill);
	}
	MatchGameState.Reset();

	Super::NativeDestruct();
}

void UEnclaveKillFeedWidget::OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState)
{
	if (AEnclaveMatchGameState* OldMatchGameState = Cast<AEnclaveMatchGameState>(OldGameState))
	{
		OldMatchGameState->OnKill.RemoveDynamic(this, &UEnclaveKillFeedWidget::HandleKill);
	}

	MatchGameState = Cast<AEnclaveMatchGameState>(NewGameState);
	if (AEnclaveMatchGameState* NewMatchGameState = MatchGameState.Get())
	{
		NewMatchGameState->OnKill.AddUniqueDynamic(this, &UEnclaveKillFeedWidget::HandleKill);
	}

	Super::OnGameStateChanged(OldGameState, NewGameState);
}

void UEnclaveKillFeedWidget::HandleKill(APlayerState* Killer, APlayerState* Victim)
{
	if (!EntryList || !EntryClass)
	{
		return;
	}

	UEnclaveKillFeedEntryWidget* Entry = CreateWidget<UEnclaveKillFeedEntryWidget>(this, EntryClass);
	if (!Entry)
	{
		return;
	}

	// The entry needs the local PlayerState to highlight you.
	Entry->CopyContextFrom(*this);
	Entry->SetKill(Killer, Victim);
	EntryList->InsertChildAt(0, Entry);

	// A dropped line is destroyed with its Slate widget (SObjectWidget calls
	// NativeDestruct), which clears its timer.
	while (EntryList->GetChildrenCount() > MaxEntries)
	{
		EntryList->RemoveChildAt(EntryList->GetChildrenCount() - 1);
	}
}
