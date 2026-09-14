// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveViewComponent.h"

#include "GameFramework/Character.h"

UEnclaveViewComponent::UEnclaveViewComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveViewComponent::InitializeComponent()
{
	Super::InitializeComponent();

	CharacterOwner = Cast<ACharacter>(GetOwner());

	RebuildInputFilters();
}

const FGameplayTagContainer& UEnclaveViewComponent::GetInputFilters() const
{
	return InputFilters;
}

bool UEnclaveViewComponent::HandleInput(const FEnclaveInputEvent& InputEvent)
{
	if (LookInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(LookInputTag))
	{
		return HandleLookInput(InputEvent);
	}

	if (AimInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(AimInputTag))
	{
		return HandleAimInput(InputEvent);
	}

	return false;
}

void UEnclaveViewComponent::RebuildInputFilters()
{
	InputFilters.Reset();

	if (LookInputTag.IsValid())
	{
		InputFilters.AddTag(LookInputTag);
	}

	if (AimInputTag.IsValid())
	{
		InputFilters.AddTag(AimInputTag);
	}
}

bool UEnclaveViewComponent::HandleLookInput(const FEnclaveInputEvent& InputEvent)
{
	const FVector2D LookValue = InputEvent.Value.Get<FVector2D>();

	if (LookValue.IsNearlyZero())
	{
		return true;
	}

	if (!IsValid(CharacterOwner))
	{
		return false;
	}

	CharacterOwner->AddControllerYawInput(LookValue.X);
	CharacterOwner->AddControllerPitchInput(LookValue.Y);

	return true;
}

bool UEnclaveViewComponent::HandleAimInput(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CharacterOwner))
	{
		return false;
	}

	switch (InputEvent.TriggerEvent)
	{
	case ETriggerEvent::Started:
		StartAiming();
		return true;

	case ETriggerEvent::Completed:
	case ETriggerEvent::Canceled:
		StopAiming();
		return true;

	default:
		return false;
	}
}

void UEnclaveViewComponent::StartAiming	()
{
}

void UEnclaveViewComponent::StopAiming()
{
}