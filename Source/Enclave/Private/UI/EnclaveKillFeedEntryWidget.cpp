// Copyright Lightcarrier Studios

#include "UI/EnclaveKillFeedEntryWidget.h"

#include "Animation/WidgetAnimation.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

void UEnclaveKillFeedEntryWidget::SetKill(APlayerState* Killer, APlayerState* Victim)
{
	// The same test as the game mode's suicide: no player behind the death, or
	// the victim themself.
	const bool bVictimOnly = !Killer || Killer == Victim;

	SetPlayerName(VictimText, Victim);

	const ESlateVisibility KillerVisibility = bVictimOnly ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
	if (KillerText)
	{
		KillerText->SetVisibility(KillerVisibility);
		if (!bVictimOnly)
		{
			SetPlayerName(KillerText, Killer);
		}
	}
	if (KillIcon)
	{
		KillIcon->SetVisibility(KillerVisibility);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DisplayTimerHandle, this, &UEnclaveKillFeedEntryWidget::HandleDisplayTimeElapsed, DisplaySeconds);
	}
}

void UEnclaveKillFeedEntryWidget::NativeDestruct()
{
	// The feed may drop this line early (too many lines) or the HUD may go.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DisplayTimerHandle);
	}

	Super::NativeDestruct();
}

void UEnclaveKillFeedEntryWidget::SetPlayerName(UTextBlock* Text, const APlayerState* PlayerState) const
{
	if (!Text)
	{
		return;
	}

	Text->SetText(PlayerState ? FText::FromString(PlayerState->GetPlayerName()) : UnknownPlayerName);

	if (PlayerState && PlayerState == GetContextPlayerState())
	{
		Text->SetColorAndOpacity(LocalPlayerColor);
	}
}

void UEnclaveKillFeedEntryWidget::HandleDisplayTimeElapsed()
{
	if (FadeOut)
	{
		// Removed in OnAnimationFinished.
		PlayAnimation(FadeOut);
	}
	else
	{
		RemoveFromParent();
	}
}

void UEnclaveKillFeedEntryWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);

	if (Animation == FadeOut)
	{
		RemoveFromParent();
	}
}
