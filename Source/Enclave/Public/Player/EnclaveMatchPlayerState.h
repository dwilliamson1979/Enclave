// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Player/EnclavePlayerState.h"
#include "EnclaveMatchPlayerState.generated.h"

/**
 * Per-player facts every match has: kills and deaths. Used with
 * AEnclaveMatchGameMode; free-for-all modes use it directly, and team modes
 * use AEnclaveTeamPlayerState. The player's score is the engine's replicated
 * APlayerState::Score.
 *
 * The match GameMode is the only writer (the setters are authority-only).
 */
UCLASS()
class ENCLAVE_API AEnclaveMatchPlayerState : public AEnclavePlayerState
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void AddKill();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetKills() const { return Kills; }

	void AddDeath();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetDeaths() const { return Deaths; }

	virtual void Reset() override;

protected:

	virtual void CopyProperties(APlayerState* PlayerState) override;

private:

	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;
};
