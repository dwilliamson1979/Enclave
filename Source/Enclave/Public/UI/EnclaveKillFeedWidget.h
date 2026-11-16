// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveKillFeedWidget.generated.h"

class AEnclaveMatchGameState;
class APlayerState;
class UEnclaveKillFeedEntryWidget;
class UPanelWidget;

/**
 * The kill feed, shared by every mode. Adds one EntryClass line per
 * AEnclaveMatchGameState::OnKill, newest first, and drops the oldest past
 * MaxEntries; each line removes itself when its time is up. Starts empty: a
 * late joiner or a new match (the HUD is recreated) shows no earlier kills.
 *
 * The Widget Blueprint must contain a panel named EntryList (a Vertical Box).
 * Set EntryClass to a UEnclaveKillFeedEntryWidget Blueprint.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveKillFeedWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UPanelWidget> EntryList;

	UPROPERTY(EditAnywhere, Category = "Kill Feed")
	TSubclassOf<UEnclaveKillFeedEntryWidget> EntryClass;

	/** Lines shown at once; a new kill past this drops the oldest line early. */
	UPROPERTY(EditAnywhere, Category = "Kill Feed", meta = (ClampMin = "1"))
	int32 MaxEntries = 5;

private:
	UFUNCTION()
	void HandleKill(APlayerState* Killer, APlayerState* Victim);

	TWeakObjectPtr<AEnclaveMatchGameState> MatchGameState;
};
