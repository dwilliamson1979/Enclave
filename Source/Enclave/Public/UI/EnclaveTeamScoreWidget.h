// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveTeamScoreWidget.generated.h"

class AEnclaveTeamGameState;
class AEnclaveTeamPlayerState;
class UPanelWidget;
class UTextBlock;

/**
 * One team's score in UEnclaveTeamScoreWidget. Created by that widget, one per
 * team; it only shows what it is given.
 *
 * The Widget Blueprint must contain a TextBlock named ScoreText, and may
 * contain one named TeamNameText. OnTeamSet is where the Blueprint styles the
 * entry (the local player's team stands out; team colors come with C5).
 */
UCLASS(Abstract, meta = (DisableNativeTick))
class ENCLAVE_API UEnclaveTeamScoreEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTeam(uint8 InTeamId, bool bInIsLocalTeam);
	void SetScore(int32 Score);

	uint8 GetTeamId() const { return TeamId; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD")
	void OnTeamSet(uint8 InTeamId, bool bInIsLocalTeam);

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TeamNameText;

	/** {0} is the team number, counted from 1. Replaced by team names with C5. */
	UPROPERTY(EditAnywhere, Category = "Score")
	FText TeamNameFormat = NSLOCTEXT("EnclaveHUD", "TeamName", "Team {0}");

private:
	uint8 TeamId = 255;
	bool bIsLocalTeam = false;
	int32 ShownScore = INDEX_NONE;
};

/**
 * Team modes' scores: one entry per team, in team order, and the score limit.
 * Used only by team-mode layouts; solo modes have UEnclaveSoloScoreWidget, so
 * neither carries the other's concepts.
 *
 * Updates on AEnclaveTeamGameState::OnTeamScoresChanged and
 * AEnclaveMatchGameState::OnScoreLimitChanged, and re-marks the local team on
 * the local PlayerState's OnTeamChanged (teams are assigned, and can be
 * rebalanced, after the HUD exists).
 *
 * The Widget Blueprint must contain a panel named TeamList (a Horizontal Box,
 * say) and may contain a TextBlock named ScoreLimitText, hidden when there is
 * no score limit. Set EntryClass to a UEnclaveTeamScoreEntryWidget Blueprint.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveTeamScoreWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState) override;
	virtual void OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UPanelWidget> TeamList;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreLimitText;

	UPROPERTY(EditAnywhere, Category = "Score")
	TSubclassOf<UEnclaveTeamScoreEntryWidget> EntryClass;

	/** {0} is the score limit. */
	UPROPERTY(EditAnywhere, Category = "Score")
	FText ScoreLimitFormat = NSLOCTEXT("EnclaveHUD", "ScoreLimit", "First to {0}");

private:
	UFUNCTION()
	void HandleTeamScoresChanged();

	UFUNCTION()
	void HandleScoreLimitChanged();

	UFUNCTION()
	void HandleTeamChanged(AEnclaveTeamPlayerState* PlayerState, uint8 NewTeamId);

	void BindGameState(AEnclaveTeamGameState* GameState);
	void UnbindGameState();
	void BindPlayerState(AEnclaveTeamPlayerState* PlayerState);
	void UnbindPlayerState();

	/** Makes one entry per team (when the count changed) and sets every score. */
	void RefreshScores();

	/** Tells each entry whether it is the local player's team. */
	void RefreshLocalTeam();

	uint8 GetLocalTeamId() const;

	TWeakObjectPtr<AEnclaveTeamGameState> TeamGameState;
	TWeakObjectPtr<AEnclaveTeamPlayerState> LocalPlayerState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEnclaveTeamScoreEntryWidget>> Entries;
};
