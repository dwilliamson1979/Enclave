// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveKillFeedEntryWidget.generated.h"

class APlayerState;
class UTextBlock;
class UWidgetAnimation;

/**
 * One line of the kill feed: "Killer ▸ Victim", or the victim alone for a
 * suicide or a death with no player behind it. Created by the feed, which
 * copies its context and calls SetKill. The local player's name takes
 * LocalPlayerColor; other names keep the Widget Blueprint's style.
 *
 * Removes itself after DisplaySeconds (a timer, no tick), playing FadeOut
 * first if the Blueprint has one. A player not yet replicated to this client
 * arrives null (the kill RPC doesn't wait for it) and shows UnknownPlayerName.
 *
 * The Widget Blueprint must contain a TextBlock named VictimText, and may
 * contain a TextBlock named KillerText (collapsed for a suicide), any widget
 * named KillIcon between them (collapsed with it), and an animation named
 * FadeOut.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveKillFeedEntryWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

public:
	/** Fills the line and starts its lifetime. Call once, after CopyContextFrom. */
	void SetKill(APlayerState* Killer, APlayerState* Victim);

protected:
	virtual void NativeDestruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UTextBlock> VictimText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> KillerText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UWidget> KillIcon;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "HUD", meta = (BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> FadeOut;

	/** Seconds the line stays before it fades (or goes, without FadeOut). */
	UPROPERTY(EditAnywhere, Category = "Kill Feed", meta = (ClampMin = "0.1", Units = "s"))
	float DisplaySeconds = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Kill Feed")
	FSlateColor LocalPlayerColor = FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f));

	UPROPERTY(EditAnywhere, Category = "Kill Feed")
	FText UnknownPlayerName = NSLOCTEXT("EnclaveHUD", "KillFeedUnknownPlayer", "Unknown");

private:
	void SetPlayerName(UTextBlock* Text, const APlayerState* PlayerState) const;
	void HandleDisplayTimeElapsed();

	FTimerHandle DisplayTimerHandle;
};
