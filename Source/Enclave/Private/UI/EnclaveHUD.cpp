// Copyright Lightcarrier Studios

#include "UI/EnclaveHUD.h"

#include "Engine/Canvas.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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
