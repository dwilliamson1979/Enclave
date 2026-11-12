// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "EnclaveTeamPlayerStart.generated.h"

/**
 * A start that belongs to one team. In a team mode only that team's players
 * spawn here (AEnclaveTeamMatchGameMode::IsStartAllowedFor); a plain
 * APlayerStart is shared by every team. Modes without teams treat it as a
 * plain start, so one map can serve both.
 */
UCLASS()
class ENCLAVE_API AEnclaveTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	AEnclaveTeamPlayerStart(const FObjectInitializer& ObjectInitializer);

	uint8 GetTeamId() const { return TeamId; }

protected:
	/** The team that spawns here, numbered like AEnclaveTeamPlayerState (0..NumTeams-1). */
	UPROPERTY(EditInstanceOnly, Category = "Team", meta = (ClampMin = "0", ClampMax = "253"))
	uint8 TeamId = 0;
};
