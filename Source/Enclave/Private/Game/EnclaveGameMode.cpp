// Copyright Lightcarrier Studios

#include "Game/EnclaveGameMode.h"

#include "Combat/Components/EnclaveCombatPolicyComponent.h"
#include "Player/EnclavePlayerState.h"
#include "Game/EnclaveGameState.h"
#include "UI/EnclaveHUD.h"

AEnclaveGameMode::AEnclaveGameMode()
{
	PlayerStateClass = AEnclavePlayerState::StaticClass();
	GameStateClass = AEnclaveGameState::StaticClass();
	HUDClass = AEnclaveHUD::StaticClass();

	CombatPolicy = CreateDefaultSubobject<UEnclaveCombatPolicyComponent>(TEXT("CombatPolicy"));
}

void AEnclaveGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	CombatPolicy->ApplyOptions(Options);
}
