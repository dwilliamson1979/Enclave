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

	return false;
}

void UEnclaveViewComponent::RebuildInputFilters()
{
	InputFilters.Reset();

	if (LookInputTag.IsValid())
	{
		InputFilters.AddTag(LookInputTag);
	}
}

bool UEnclaveViewComponent::HandleLookInput(const FEnclaveInputEvent& InputEvent)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return false;
	}

	const FVector2D LookValue = InputEvent.Value.Get<FVector2D>();
	if (LookValue.IsNearlyZero())
	{
		return true;
	}

	Character->AddControllerYawInput(LookValue.X);
	Character->AddControllerPitchInput(LookValue.Y);

	return true;
}
