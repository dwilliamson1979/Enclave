// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "EnclaveGameState.generated.h"

/**
 * Root GameState for every Enclave map, used with AEnclaveGameMode (the SSR test
 * range uses it directly). It holds only what every map needs, which is nothing
 * beyond AGameState yet.
 *
 * The GameState tree mirrors the GameMode tree one level for one level, and each
 * GameMode sets GameStateClass to its own level (the PlayerState tree does the
 * same; see AEnclavePlayerState):
 *
 *   AEnclaveGameState            AEnclaveGameMode            every map
 *     AEnclaveMatchGameState       AEnclaveMatchGameMode       a contest
 *       AEnclaveSoloGameState        AEnclaveSoloMatchGameMode   free-for-all
 *       AEnclaveTeamGameState        AEnclaveTeamMatchGameMode   teams
 *
 * A field belongs at the level where every mode below it has that concept.
 */
UCLASS()
class ENCLAVE_API AEnclaveGameState : public AGameState
{
	GENERATED_BODY()
};
