// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveSoloScoreWidget.generated.h"

class AEnclaveMatchGameState;
class UTextBlock;

/**
 * Free-for-all scores: your score next to your rival's, the best score among
 * the other players. While someone else leads, the rival is the leader; while
 * you lead, it is second place, so you see your margin either way. A tie for
 * the top shows the other player as the leader. Used only by solo-mode
 * layouts; team modes have UEnclaveTeamScoreWidget.
 *
 * Recomputed on AEnclaveMatchGameState::OnPlayerScoreChanged and
 * OnPlayersChanged (a join or a leave can change the rival). Spectators are
 * left out, and a spectating local player sees only the leader.
 *
 * The Widget Blueprint must contain TextBlocks named PlayerScoreText and
 * RivalScoreText, and may contain one named RivalLabelText ("Leader" or
 * "2nd"). The rival is hidden while nobody else plays. OnLeadChanged is
 * where the Blueprint styles the lead.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveSoloScoreWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState) override;
	virtual void OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState) override;

	/** The local player took or lost the lead; also called with the first scores shown. */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnLeadChanged(bool bIsLeading);

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerScoreText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UTextBlock> RivalScoreText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RivalLabelText;

	/** The rival while someone else leads (or ties for the lead). */
	UPROPERTY(EditAnywhere, Category = "Score")
	FText LeaderLabel = NSLOCTEXT("EnclaveHUD", "Leader", "Leader");

	/** The rival while the local player leads. */
	UPROPERTY(EditAnywhere, Category = "Score")
	FText SecondLabel = NSLOCTEXT("EnclaveHUD", "Second", "2nd");

private:
	UFUNCTION()
	void HandlePlayerScoreChanged(APlayerState* PlayerState);

	UFUNCTION()
	void HandlePlayersChanged();

	void BindGameState(AEnclaveMatchGameState* GameState);
	void UnbindGameState();

	/** Finds the rival and shows both scores. */
	void RefreshScores();

	static void ShowScore(UTextBlock* Text, float Score, bool bVisible);

	TWeakObjectPtr<AEnclaveMatchGameState> MatchGameState;
	TWeakObjectPtr<APlayerState> LocalPlayerState;

	/** Last value sent to OnLeadChanged; unset until the first call. */
	TOptional<bool> bWasLeading;
};
