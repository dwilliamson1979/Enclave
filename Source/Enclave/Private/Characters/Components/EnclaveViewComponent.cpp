// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveViewComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Equipment/Items/EnclaveItem.h"

UEnclaveViewComponent::UEnclaveViewComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEnclaveViewComponent::InitializeComponent()
{
	Super::InitializeComponent();
	CharacterOwner = Cast<ACharacter>(GetOwner());
	RebuildInputFilters();
}

void UEnclaveViewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetFocus(/*bImmediate=*/true);
	Super::EndPlay(EndPlayReason);
}

void UEnclaveViewComponent::SetViewCamera(UCameraComponent* InCamera)
{
	ViewCamera = InCamera;
}

const FGameplayTagContainer& UEnclaveViewComponent::GetInputFilters() const
{
	return InputFilters;
}

bool UEnclaveViewComponent::HandleInput(const FEnclaveInputEvent& InputEvent)
{
	return LookInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(LookInputTag)
		? HandleLookInput(InputEvent)
		: false;
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
	if (!IsValid(CharacterOwner))
	{
		return false;
	}

	const FVector2D LookValue = InputEvent.Value.Get<FVector2D>();
	if (LookValue.IsNearlyZero())
	{
		return true;
	}

	CharacterOwner->AddControllerYawInput(LookValue.X * CurrentSensitivityMultiplier);
	CharacterOwner->AddControllerPitchInput(LookValue.Y * CurrentSensitivityMultiplier);
	return true;
}

void UEnclaveViewComponent::BeginFocus(AEnclaveItem* SourceItem, const FEnclaveFocusViewSettings& Settings)
{
	if (!IsValid(SourceItem) || !IsValid(CharacterOwner) || !CharacterOwner->IsLocallyControlled() || !IsValid(ViewCamera))
	{
		return;
	}

	if (!bHasBaseFOV)
	{
		BaseFOV = ViewCamera->FieldOfView;
		bHasBaseFOV = true;
	}

	bFocusActive = true;
	FocusSourceItem = SourceItem;
	FocusTransitionDuration = FMath::Max(0.0f, Settings.TransitionDuration);
	BeginViewBlend(FMath::Clamp(Settings.FocusFOV, 5.0f, 170.0f),
		FMath::Max(0.01f, Settings.LookSensitivityMultiplier), FocusTransitionDuration);
}

void UEnclaveViewComponent::EndFocus(AEnclaveItem* SourceItem)
{
	if (!bFocusActive || FocusSourceItem.Get() != SourceItem)
	{
		return; // A late cue from an old item cannot end the new item's Focus.
	}

	ResetFocus();
}

void UEnclaveViewComponent::ResetFocus(bool bImmediate)
{
	bFocusActive = false;
	FocusSourceItem.Reset();

	if (!bHasBaseFOV)
	{
		return;
	}

	if (bImmediate || !IsValid(ViewCamera) || !IsValid(CharacterOwner) || !CharacterOwner->IsLocallyControlled())
	{
		if (IsValid(ViewCamera) && IsValid(CharacterOwner) && CharacterOwner->IsLocallyControlled())
		{
			ViewCamera->SetFieldOfView(BaseFOV);
		}
		CurrentSensitivityMultiplier = 1.0f;
		bHasBaseFOV = false;
		SetComponentTickEnabled(false);
		return;
	}

	BeginViewBlend(BaseFOV, 1.0f, FocusTransitionDuration);
}

void UEnclaveViewComponent::BeginViewBlend(float InFOV, float InSensitivity, float Duration)
{
	if (!IsValid(ViewCamera))
	{
		return;
	}

	BlendStartFOV = ViewCamera->FieldOfView;
	BlendStartSensitivity = CurrentSensitivityMultiplier;
	BlendTargetFOV = InFOV;
	BlendTargetSensitivity = InSensitivity;
	BlendDuration = Duration;
	BlendElapsed = 0.0f;

	if (Duration <= KINDA_SMALL_NUMBER)
	{
		ViewCamera->SetFieldOfView(BlendTargetFOV);
		CurrentSensitivityMultiplier = BlendTargetSensitivity;
		if (!bFocusActive)
		{
			bHasBaseFOV = false;
		}
		SetComponentTickEnabled(false);
		return;
	}

	SetComponentTickEnabled(true);
}

void UEnclaveViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsValid(ViewCamera))
	{
		SetComponentTickEnabled(false);
		return;
	}

	BlendElapsed += DeltaTime;
	const float Alpha = BlendDuration > KINDA_SMALL_NUMBER ? FMath::Clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f) : 1.0f;
	ViewCamera->SetFieldOfView(FMath::Lerp(BlendStartFOV, BlendTargetFOV, Alpha));
	CurrentSensitivityMultiplier = FMath::Lerp(BlendStartSensitivity, BlendTargetSensitivity, Alpha);

	if (Alpha >= 1.0f)
	{
		SetComponentTickEnabled(false);
		if (!bFocusActive)
		{
			bHasBaseFOV = false;
		}
	}
}
