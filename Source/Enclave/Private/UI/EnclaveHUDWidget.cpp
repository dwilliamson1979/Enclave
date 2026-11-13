// Copyright Lightcarrier Studios

#include "UI/EnclaveHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// Each setter stores first, so a handler (and every child) already sees the
// new value through the getters, then notifies itself, then its children.
// The unchanged check also covers a named slot's content, which both the
// widget that fills the slot and the widget that owns it can reach.

void UEnclaveHUDWidget::SetContextGameState(AGameStateBase* NewGameState)
{
	AGameStateBase* OldGameState = ContextGameState.Get();
	if (OldGameState == NewGameState)
	{
		return;
	}

	ContextGameState = NewGameState;
	OnGameStateChanged(OldGameState, NewGameState);
	ForEachChildHUDWidget(WidgetTree, [NewGameState](UEnclaveHUDWidget& Child) { Child.SetContextGameState(NewGameState); });
}

void UEnclaveHUDWidget::SetContextPlayerState(APlayerState* NewPlayerState)
{
	APlayerState* OldPlayerState = ContextPlayerState.Get();
	if (OldPlayerState == NewPlayerState)
	{
		return;
	}

	ContextPlayerState = NewPlayerState;
	OnPlayerStateChanged(OldPlayerState, NewPlayerState);
	ForEachChildHUDWidget(WidgetTree, [NewPlayerState](UEnclaveHUDWidget& Child) { Child.SetContextPlayerState(NewPlayerState); });
}

void UEnclaveHUDWidget::SetContextPawn(APawn* NewPawn)
{
	APawn* OldPawn = ContextPawn.Get();
	if (OldPawn == NewPawn)
	{
		return;
	}

	ContextPawn = NewPawn;
	OnPawnChanged(OldPawn, NewPawn);
	ForEachChildHUDWidget(WidgetTree, [NewPawn](UEnclaveHUDWidget& Child) { Child.SetContextPawn(NewPawn); });
}

void UEnclaveHUDWidget::CopyContextFrom(const UEnclaveHUDWidget& Source)
{
	// Same order as AEnclaveHUD: match, then player, then pawn.
	SetContextGameState(Source.GetContextGameState());
	SetContextPlayerState(Source.GetContextPlayerState());
	SetContextPawn(Source.GetContextPawn());
}

void UEnclaveHUDWidget::OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState)
{
	K2_OnGameStateChanged(OldGameState, NewGameState);
}

void UEnclaveHUDWidget::OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState)
{
	K2_OnPlayerStateChanged(OldPlayerState, NewPlayerState);
}

void UEnclaveHUDWidget::OnPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	K2_OnPawnChanged(OldPawn, NewPawn);
}

void UEnclaveHUDWidget::ForEachChildHUDWidget(const UWidgetTree* Tree, TFunctionRef<void(UEnclaveHUDWidget&)> Func)
{
	if (!Tree)
	{
		return;
	}

	// ForEachWidget walks panels and named slots but not into a nested user
	// widget's own tree (WidgetTree.cpp, ForWidgetAndChildren).
	Tree->ForEachWidget([&Func](UWidget* Widget)
	{
		if (UEnclaveHUDWidget* HUDWidget = Cast<UEnclaveHUDWidget>(Widget))
		{
			Func(*HUDWidget);
		}
		else if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
		{
			ForEachChildHUDWidget(UserWidget->WidgetTree, Func);
		}
	});
}
