// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EnclaveHUD.generated.h"

class AGameStateBase;
class APawn;
class APlayerState;
class UEnclaveHUDWidget;

/**
 * Local player's HUD. One per local PlayerController, spawned by
 * ClientSetHUD (PlayerController.cpp, ClientSetHUD_Implementation) when the
 * player joins and again after each seamless travel, so it lives for one match
 * and survives respawns. Never exists on a dedicated server.
 *
 * Owns the root layout widget (LayoutClass, set in the HUD Blueprint) and is
 * the one place that knows when match data arrives: it watches the world's
 * GameState, the controller's PlayerState and its possessed pawn, and pushes
 * each into the layout, which passes it down to every HUD widget.
 *
 * All player-facing UI is UMG. The canvas draws only the temporary reticle
 * (and later debug); firing, traces, abilities and SSR never depend on it.
 */
UCLASS()
class ENCLAVE_API AEnclaveHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UEnclaveHUDWidget* GetLayout() const { return Layout; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Root widget of the in-match HUD. None = no UMG HUD (the reticle still draws). */
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	TSubclassOf<UEnclaveHUDWidget> LayoutClass;

private:
	void HandleGameStateSet(AGameStateBase* NewGameState);
	void HandlePlayerStateChanged(APlayerState* NewPlayerState);

	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UPROPERTY(Transient)
	TObjectPtr<UEnclaveHUDWidget> Layout;

	FDelegateHandle GameStateSetHandle;
	FDelegateHandle PlayerStateChangedHandle;
};
