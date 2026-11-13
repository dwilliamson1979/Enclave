// Copyright Lightcarrier Studios

#include "UI/EnclaveHUD.h"

#include "Controllers/EnclavePlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/EnclaveHUDWidget.h"

void AEnclaveHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController() || !LayoutClass)
	{
		return;
	}

	Layout = CreateWidget<UEnclaveHUDWidget>(PlayerOwner, LayoutClass);
	if (!Layout)
	{
		return;
	}
	// The owning player's part of the screen (all of it outside split screen).
	Layout->AddToPlayerScreen();

	// Watch for each piece arriving or changing. On a client any of them may
	// still be on its way; the pawn also changes on every death and respawn.
	// OnPossessedPawnChanged fires on the server from Possess/UnPossess and on
	// the owning client from OnRep_Pawn (Controller.cpp, OnRep_Pawn).
	UWorld* World = GetWorld();
	GameStateSetHandle = World->GameStateSetEvent.AddUObject(this, &AEnclaveHUD::HandleGameStateSet);
	if (AEnclavePlayerController* EnclaveController = Cast<AEnclavePlayerController>(PlayerOwner))
	{
		PlayerStateChangedHandle = EnclaveController->OnPlayerStateChanged.AddUObject(this, &AEnclaveHUD::HandlePlayerStateChanged);
	}
	PlayerOwner->OnPossessedPawnChanged.AddDynamic(this, &AEnclaveHUD::HandlePossessedPawnChanged);

	// Then push what is already here: match, then player, then pawn, so a
	// widget reacting to the pawn can already read the match.
	Layout->SetContextGameState(World->GetGameState());
	Layout->SetContextPlayerState(PlayerOwner->PlayerState);
	Layout->SetContextPawn(PlayerOwner->GetPawn());
}

void AEnclaveHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.Remove(GameStateSetHandle);
	}
	GameStateSetHandle.Reset();

	if (PlayerOwner)
	{
		if (AEnclavePlayerController* EnclaveController = Cast<AEnclavePlayerController>(PlayerOwner))
		{
			EnclaveController->OnPlayerStateChanged.Remove(PlayerStateChangedHandle);
		}
		PlayerOwner->OnPossessedPawnChanged.RemoveDynamic(this, &AEnclaveHUD::HandlePossessedPawnChanged);
	}
	PlayerStateChangedHandle.Reset();

	if (Layout)
	{
		Layout->RemoveFromParent();
		Layout = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AEnclaveHUD::HandleGameStateSet(AGameStateBase* NewGameState)
{
	if (Layout)
	{
		Layout->SetContextGameState(NewGameState);
	}
}

void AEnclaveHUD::HandlePlayerStateChanged(APlayerState* NewPlayerState)
{
	if (Layout)
	{
		Layout->SetContextPlayerState(NewPlayerState);
	}
}

void AEnclaveHUD::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	if (Layout)
	{
		Layout->SetContextPawn(NewPawn);
	}
}

void AEnclaveHUD::DrawHUD()
{
	Super::DrawHUD();

	// AHUD is a local presentation actor. Never draw a reticle for an
	// unpossessed controller, a remote pawn, or a non-rendering server.
	if (!Canvas || !PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	const APawn* ControlledPawn = PlayerOwner->GetPawn();
	if (!IsValid(ControlledPawn) || !ControlledPawn->IsLocallyControlled())
	{
		return;
	}

	const float CenterX = static_cast<float>(Canvas->SizeX) * 0.5f;
	const float CenterY = static_cast<float>(Canvas->SizeY) * 0.5f;

	// Fixed pixel-size reticle for the initial FPS HUD. Use a dark backing
	// under each white stroke so it remains legible against bright scenes.
	constexpr float Gap = 5.0f;
	constexpr float ArmLength = 10.0f;
	constexpr float OutlineThickness = 4.0f;
	constexpr float StrokeThickness = 2.0f;

	const auto DrawArm = [this](float StartX, float StartY, float EndX, float EndY)
	{
		DrawLine(StartX, StartY, EndX, EndY, FLinearColor::Black, OutlineThickness);
		DrawLine(StartX, StartY, EndX, EndY, FLinearColor::White, StrokeThickness);
	};

	DrawArm(CenterX - Gap - ArmLength, CenterY, CenterX - Gap, CenterY);
	DrawArm(CenterX + Gap, CenterY, CenterX + Gap + ArmLength, CenterY);
	DrawArm(CenterX, CenterY - Gap - ArmLength, CenterX, CenterY - Gap);
	DrawArm(CenterX, CenterY + Gap, CenterX, CenterY + Gap + ArmLength);

	// Two-pixel center dot, also backed for contrast.
	DrawRect(FLinearColor::Black, CenterX - 2.0f, CenterY - 2.0f, 4.0f, 4.0f);
	DrawRect(FLinearColor::White, CenterX - 1.0f, CenterY - 1.0f, 2.0f, 2.0f);
}
