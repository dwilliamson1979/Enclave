// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "EnclaveGameMode.generated.h"

class UEnclaveCombatPolicyComponent;

/**
 * Shared Enclave game mode: owns the combat policy (hit detection mode and
 * server-side rewind, in UEnclaveCombatPolicyComponent) for every map, and sets
 * the default PlayerState, GameState and HUD classes. It has no match rules, so
 * test maps such as the SSR test range use it directly. Match rules live in
 * AEnclaveMatchGameMode.
 */
UCLASS()
class ENCLAVE_API AEnclaveGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AEnclaveGameMode();

	/**
	 * Server: replaces the controller's pawn with a fresh one at a player start.
	 * Works for any controller (players now, bots later). The old pawn cleans
	 * itself up when it is unpossessed (see AEnclaveCharacter::UnPossessed).
	 * No rules: the caller decides when a respawn is allowed.
	 */
	virtual void RespawnPlayer(AController* Controller);

protected:
	/**
	 * Server: unpossesses and destroys the controller's pawn, if any. Unpossess
	 * first: the pawn's UnPossessed does its teardown while it can still reach
	 * the PlayerState's ASC.
	 */
	void RemovePawn(AController& Controller);

	/** Passes the server travel/startup options to the combat policy. */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

private:
	/** Server-only combat policy. Callers use UEnclaveCombatPolicyComponent::Get(World). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveCombatPolicyComponent> CombatPolicy;
};
