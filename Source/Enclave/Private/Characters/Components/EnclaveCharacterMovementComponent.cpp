// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveCharacterMovementComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"

UEnclaveCharacterMovementComponent::UEnclaveCharacterMovementComponent()
{
	bWantsInitializeComponent = true;
	GetNavAgentPropertiesRef().bCanCrouch = true;
}

void UEnclaveCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();

	RebuildInputFilters();
}

bool UEnclaveCharacterMovementComponent::IsInMovementMode(EMovementMode InMovementMode) const
{
	return MovementMode == InMovementMode;
}

bool UEnclaveCharacterMovementComponent::IsLocomotionEnabled() const
{
	return IsActive() && !IsInMovementMode(MOVE_None);
}

const FGameplayTagContainer& UEnclaveCharacterMovementComponent::GetInputFilters() const
{
	return InputFilters;
}

bool UEnclaveCharacterMovementComponent::HandleInput(const FEnclaveInputEvent& InputEvent)
{
	if (MoveInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(MoveInputTag))
	{
		return HandleMoveInput(InputEvent);
	}

	if (JumpInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(JumpInputTag))
	{
		return HandleJumpInput(InputEvent);
	}

	if (CrouchInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(CrouchInputTag))
	{
		return HandleCrouchInput(InputEvent);
	}

	return false;
}

void UEnclaveCharacterMovementComponent::RebuildInputFilters()
{
	InputFilters.Reset();

	if (MoveInputTag.IsValid())
	{
		InputFilters.AddTag(MoveInputTag);
	}

	if (JumpInputTag.IsValid())
	{
		InputFilters.AddTag(JumpInputTag);
	}

	if (CrouchInputTag.IsValid())
	{
		InputFilters.AddTag(CrouchInputTag);
	}
}

bool UEnclaveCharacterMovementComponent::HandleMoveInput(const FEnclaveInputEvent& InputEvent)
{
	const FVector2D MoveValue = InputEvent.Value.Get<FVector2D>();
	if (MoveValue.IsNearlyZero())
	{
		return true;
	}

	ACharacter* Character = GetCharacterOwner();
	if (!Character)
	{
		return false;
	}

	const AController* Controller = Character->GetController();
	if (!Controller)
	{
		return false;
	}

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawRotation(0.0f, ControlRotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	Character->AddMovementInput(ForwardDirection, MoveValue.Y);
	Character->AddMovementInput(RightDirection, MoveValue.X);

	return true;
}

bool UEnclaveCharacterMovementComponent::HandleJumpInput(const FEnclaveInputEvent& InputEvent)
{
	ACharacter* Character = GetCharacterOwner();
	if (!Character)
	{
		return false;
	}

	switch (InputEvent.TriggerEvent)
	{
	case ETriggerEvent::Started:
	case ETriggerEvent::Triggered:
		Character->Jump();
		return true;

	case ETriggerEvent::Completed:
	case ETriggerEvent::Canceled:
		Character->StopJumping();
		return true;

	default:
		return false;
	}
}

bool UEnclaveCharacterMovementComponent::HandleCrouchInput(const FEnclaveInputEvent& InputEvent)
{
	ACharacter* Character = GetCharacterOwner();
	if (!Character)
	{
		return false;
	}

	switch (InputEvent.TriggerEvent)
	{
	case ETriggerEvent::Started:
	case ETriggerEvent::Triggered:
		Character->Crouch();
		return true;

	case ETriggerEvent::Completed:
	case ETriggerEvent::Canceled:
		Character->UnCrouch();
		return true;

	default:
		return false;
	}
}
