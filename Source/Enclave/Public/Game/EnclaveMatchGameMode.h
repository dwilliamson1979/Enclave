// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameMode.h"
#include "Game/EnclaveMatchSettings.h"
#include "EnclaveMatchGameMode.generated.h"

class AEnclaveMatchGameState;

/**
 * Match framework shared by every mode: phases (warmup, in progress, post-match),
 * the time limit, death handling, respawn and restart. It has no rules of its
 * own; each mode is a subclass that overrides the rule hooks.
 *
 * It holds only what every match has, so it knows nothing about teams or about
 * who won. Modes derive from AEnclaveSoloMatchGameMode (free-for-all) or
 * AEnclaveTeamMatchGameMode, which own their results. Uses AEnclaveMatchGameState.
 *
 * Phases come from AGameMode's match state machine: this class answers
 * ReadyToStartMatch / ReadyToEndMatch and reacts to the Handle* events.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveMatchGameMode : public AEnclaveGameMode
{
	GENERATED_BODY()

public:
	AEnclaveMatchGameMode();

	const FEnclaveMatchSettings& GetMatchSettings() const { return MatchSettings; }

protected:
	/** Applies match URL options on top of the defaults. See FEnclaveMatchSettings. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	/**
	 * Polled every frame while WaitingToStart. Runs the warmup countdown once
	 * MinPlayersToStart are in, cancels it if the count drops below that, and
	 * returns true when the countdown has finished.
	 */
	virtual bool ReadyToStartMatch_Implementation() override;

	/**
	 * Warmup is over: sets the time limit and then calls Super, which spawns
	 * every player. Subclasses that add their own match state (results, team
	 * scores) set it up before calling Super.
	 */
	virtual void HandleMatchHasStarted() override;

	/** Defaults for this mode. Subclasses set their own in their constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	FEnclaveMatchSettings MatchSettings;

private:
	/** Cancels a running warmup countdown, if any. */
	void CancelWarmup(AEnclaveMatchGameState& MatchGameState);

	/** Server time the warmup ends. Unset while there aren't enough players. */
	TOptional<double> WarmupEndServerTime;
};
