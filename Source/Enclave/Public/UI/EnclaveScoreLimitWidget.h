// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveScoreLimitWidget.generated.h"

class AEnclaveMatchGameState;
class UTextBlock;

/**
 * The match's score limit ("First to 25"), shared by every mode: it lives in
 * the base layout, not in each mode's score widget. Hidden when there is no
 * limit. Updates on AEnclaveMatchGameState::OnScoreLimitChanged.
 *
 * The Widget Blueprint must contain a TextBlock named LimitText.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveScoreLimitWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UTextBlock> LimitText;

	/** {0} is the score limit. */
	UPROPERTY(EditAnywhere, Category = "Score")
	FText LimitFormat = NSLOCTEXT("EnclaveHUD", "ScoreLimit", "First to {0}");

private:
	UFUNCTION()
	void HandleScoreLimitChanged();

	TWeakObjectPtr<AEnclaveMatchGameState> MatchGameState;
};
