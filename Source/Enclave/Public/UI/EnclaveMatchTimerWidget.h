// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveMatchTimerWidget.generated.h"

class AEnclaveMatchGameState;
class UTextBlock;

/**
 * The match phase and its countdown, shared by every mode: "Waiting for
 * players", "Match starts in 0:05", the time limit, "Next match in 0:08".
 *
 * The phase label changes on AEnclaveMatchGameState::OnPhaseChanged; the
 * countdown is computed each tick from PhaseEndServerTime and the server
 * clock (no per-second replication) and the text is set only when the shown
 * second changes. The time is hidden when the phase has no end.
 *
 * The Widget Blueprint must contain two TextBlocks named PhaseText and
 * TimeText. A label left empty hides PhaseText for that phase.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveMatchTimerWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> PhaseText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> TimeText;

	/** WaitingToStart with no countdown (not enough players yet). */
	UPROPERTY(EditAnywhere, Category = "Timer")
	FText WaitingForPlayersLabel = NSLOCTEXT("EnclaveHUD", "WaitingForPlayers", "Waiting for players");

	/** WaitingToStart with a countdown (warmup). */
	UPROPERTY(EditAnywhere, Category = "Timer")
	FText WarmupLabel = NSLOCTEXT("EnclaveHUD", "Warmup", "Match starts in");

	/** InProgress. Empty by default: the time limit alone is clear. */
	UPROPERTY(EditAnywhere, Category = "Timer")
	FText InProgressLabel;

	/** WaitingPostMatch. */
	UPROPERTY(EditAnywhere, Category = "Timer")
	FText PostMatchLabel = NSLOCTEXT("EnclaveHUD", "PostMatch", "Next match in");

private:
	UFUNCTION()
	void HandlePhaseChanged();

	/** Sets the time text when the shown second changes. */
	void RefreshTime();

	TWeakObjectPtr<AEnclaveMatchGameState> MatchGameState;

	/** Seconds currently shown; INDEX_NONE while the time is hidden. */
	int32 ShownSeconds = INDEX_NONE;
};
