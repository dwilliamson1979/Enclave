// Copyright Lightcarrier Studios

#include "Game/EnclaveGameMode.h"

#include "Combat/Components/EnclaveCombatPolicyComponent.h"
#include "Player/EnclavePlayerState.h"
#include "Game/EnclaveGameState.h"
#include "GameFramework/Pawn.h"
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

void AEnclaveGameMode::RespawnPlayer(AController* Controller)
{
	if (!IsValid(Controller))
	{
		return;
	}

	// RestartPlayer reuses a pawn the controller still has, so the old one must
	// be gone first. Unpossess before Destroy: the pawn's UnPossessed does its
	// teardown while it can still reach the PlayerState's ASC.
	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	RestartPlayer(Controller);
}
