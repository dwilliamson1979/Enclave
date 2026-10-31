// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Game/EnclaveGameMode.h"
#include "Game/EnclaveMatchSettings.h"
#include "EnclaveMatchGameMode.generated.h"

/**
 * Match framework shared by every mode: phases (warmup, in progress, post-match),
 * the time limit, death handling, respawn and restart. It has no rules of its
 * own; each mode (TDM, DM, ...) is a subclass that overrides the rule hooks.
 *
 * Phases come from AGameMode's match state machine: this class answers
 * ReadyToStartMatch / ReadyToEndMatch and reacts to the Handle* events.
 */
UCLASS(Abstract)
class ENCLAVE_API AEnclaveMatchGameMode : public AEnclaveGameMode
{
	GENERATED_BODY()

public:
	const FEnclaveMatchSettings& GetMatchSettings() const { return MatchSettings; }

protected:
	/** Applies match URL options on top of the defaults. See FEnclaveMatchSettings. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual bool ReadyToStartMatch_Implementation() override;

	/** Defaults for this mode. Subclasses set their own in their constructor. */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	FEnclaveMatchSettings MatchSettings;
};
