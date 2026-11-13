// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "EnclaveHUDWidget.generated.h"

class AGameStateBase;
class APawn;
class APlayerState;
class UWidgetTree;

/**
 * Base for every in-match HUD widget. Widgets never go looking for match data:
 * AEnclaveHUD gathers the GameState, the local PlayerState and the possessed
 * pawn as each arrives or changes, and pushes them into the root layout, which
 * passes them down to every HUD widget it contains. A widget only reacts:
 * override OnGameStateChanged, OnPlayerStateChanged and OnPawnChanged (or the
 * Blueprint events of the same name).
 *
 * On a client the HUD exists before some of these replicate, so any of them
 * can be null at first and arrive later; the pawn changes on every death and
 * respawn. Each handler runs once per real change, with the old and new value.
 *
 * GetOwningPlayerPawn / GetOwningPlayerState (UUserWidget) read the controller
 * directly and give no notification; prefer the Context getters here.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Context setters. Called by AEnclaveHUD on the root layout and passed down to child HUD widgets; no-ops when unchanged. */
	void SetContextGameState(AGameStateBase* NewGameState);
	void SetContextPlayerState(APlayerState* NewPlayerState);
	void SetContextPawn(APawn* NewPawn);

	/** For HUD widgets created at runtime (a kill feed entry, say): take the creator's context. */
	void CopyContextFrom(const UEnclaveHUDWidget& Source);

	UFUNCTION(BlueprintPure, Category = "HUD")
	AGameStateBase* GetContextGameState() const { return ContextGameState.Get(); }

	UFUNCTION(BlueprintPure, Category = "HUD")
	APlayerState* GetContextPlayerState() const { return ContextPlayerState.Get(); }

	UFUNCTION(BlueprintPure, Category = "HUD")
	APawn* GetContextPawn() const { return ContextPawn.Get(); }

	template <class T>
	T* GetContextGameState() const { return Cast<T>(ContextGameState.Get()); }

	template <class T>
	T* GetContextPlayerState() const { return Cast<T>(ContextPlayerState.Get()); }

	template <class T>
	T* GetContextPawn() const { return Cast<T>(ContextPawn.Get()); }

protected:
	/** The GameState arrived. Bind to its delegates here (it lives as long as the match, so no unbind is needed). */
	virtual void OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState);

	/** The local PlayerState arrived or was replaced. Unbind from Old, bind to New. */
	virtual void OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState);

	/** Possession changed: New is null while dead or spectating. Unbind from Old, bind to New. */
	virtual void OnPawnChanged(APawn* OldPawn, APawn* NewPawn);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "On Game State Changed"))
	void K2_OnGameStateChanged(AGameStateBase* OldGameState, AGameStateBase* NewGameState);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "On Player State Changed"))
	void K2_OnPlayerStateChanged(APlayerState* OldPlayerState, APlayerState* NewPlayerState);

	UFUNCTION(BlueprintImplementableEvent, Category = "HUD", meta = (DisplayName = "On Pawn Changed"))
	void K2_OnPawnChanged(APawn* OldPawn, APawn* NewPawn);

private:
	/**
	 * Calls Func on each HUD widget in this widget's tree. Stops at a HUD widget
	 * (it passes the context on through its own tree) and looks inside plain
	 * user widgets, so a HUD widget wrapped in one still gets its context.
	 */
	static void ForEachChildHUDWidget(const UWidgetTree* Tree, TFunctionRef<void(UEnclaveHUDWidget&)> Func);

	TWeakObjectPtr<AGameStateBase> ContextGameState;
	TWeakObjectPtr<APlayerState> ContextPlayerState;
	TWeakObjectPtr<APawn> ContextPawn;
};
