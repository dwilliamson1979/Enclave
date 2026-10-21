// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveViewComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/EnclaveDebug.h"
#include "GameFramework/Character.h"
#include "Equipment/Items/EnclaveItem.h"

UEnclaveViewComponent::UEnclaveViewComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// The sight socket follows the final animated hand pose. Read it after
	// the first-person skeletal mesh's PrePhysics animation update.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
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

void UEnclaveViewComponent::SetFirstPersonMesh(USkeletalMeshComponent* InMesh)
{
	if (IsValid(FirstPersonMesh))
	{
		RemoveTickPrerequisiteComponent(FirstPersonMesh);
	}
	FirstPersonMesh = InMesh;
	if (IsValid(FirstPersonMesh))
	{
		// The sight is attached to an animated hand; sample AFTER the arms tick.
		AddTickPrerequisiteComponent(FirstPersonMesh);
	}
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
	ConfigureSightAlignment(SourceItem, Settings.SightAlignment);
	FocusTransitionDuration = FMath::Max(0.0f, Settings.TransitionDuration);
	BeginViewBlend(FMath::Clamp(Settings.FocusFOV, 5.0f, 170.0f),
		FMath::Max(0.01f, Settings.LookSensitivityMultiplier), FocusTransitionDuration);
}

void UEnclaveViewComponent::EndFocus(AEnclaveItem* SourceItem, bool bSourcePresentationRemoved)
{
	if (bFocusActive && FocusSourceItem.Get() == SourceItem)
	{
		ResetFocus();
	}

	// A late cue from an old item cannot end a newer Focus. Removal of the
	// old presentation, however, must drop its arms offset immediately:
	// the old item can remain socket-attached but hidden after a slot swap.
	if (bSourcePresentationRemoved && AlignmentSourceItem.Get() == SourceItem)
	{
		RestoreSightAlignment();
		UpdateTickEnabled();
	}
}

void UEnclaveViewComponent::ResetFocus(bool bImmediate)
{
	bFocusActive = false;
	FocusSourceItem.Reset();

	if (bImmediate || !bHasBaseFOV || !IsValid(ViewCamera) || !IsValid(CharacterOwner) || !CharacterOwner->IsLocallyControlled())
	{
		if (bHasBaseFOV && IsValid(ViewCamera))
		{
			ViewCamera->SetFieldOfView(BaseFOV);
		}
		CurrentSensitivityMultiplier = 1.0f;
		bHasBaseFOV = false;
		bViewBlendInProgress = false;
		RestoreSightAlignment();
		UpdateTickEnabled();
		return;
	}

	BeginViewBlend(BaseFOV, 1.0f, FocusTransitionDuration);
}

void UEnclaveViewComponent::ConfigureSightAlignment(AEnclaveItem* SourceItem, const FEnclaveFocusSightAlignmentSettings& Settings)
{
	if (!Settings.bEnabled)
	{
		RestoreSightAlignment();
		return;
	}

	// Re-pressing the same item during blend-out reuses the *original* hip
	// transform and continues smoothly from the current alignment fraction.
	if (bSightAlignmentBound && AlignmentSourceItem.Get() == SourceItem)
	{
		ActiveSightSettings = Settings;
		return;
	}

	RestoreSightAlignment();

	USkeletalMeshComponent* ItemMesh = IsValid(SourceItem) ? SourceItem->GetFirstPersonMesh() : nullptr;
	if (!IsValid(FirstPersonMesh) || !IsValid(ViewCamera) ||
		FirstPersonMesh->GetAttachParent() != ViewCamera ||
		!IsValid(ItemMesh) || ItemMesh->GetAttachParent() != FirstPersonMesh ||
		Settings.SightSocketName.IsNone() || !ItemMesh->DoesSocketExist(Settings.SightSocketName))
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning,
			TEXT("%s: Focus FOV is active, but physical sight alignment for %s cannot start. Check camera-attached FP arms, equipped FP item attachment, and sight socket '%s'."),
			*GetNameSafe(GetOwner()), *GetNameSafe(SourceItem), *Settings.SightSocketName.ToString());
		return;
	}

	BaseFirstPersonMeshRelativeTransform = FirstPersonMesh->GetRelativeTransform();
	AlignmentSourceItem = SourceItem;
	ActiveSightSettings = Settings;
	bSightAlignmentBound = true;
	bHasSightCalibration = false;
}

void UEnclaveViewComponent::RestoreSightAlignment()
{
	if (bSightAlignmentBound && IsValid(FirstPersonMesh))
	{
		FirstPersonMesh->SetRelativeTransform(BaseFirstPersonMeshRelativeTransform);
	}

	bSightAlignmentBound = false;
	bHasSightCalibration = false;
	AlignmentSourceItem.Reset();
	CurrentSightAlignmentAlpha = 0.0f;
	BlendStartSightAlignmentAlpha = 0.0f;
	BlendTargetSightAlignmentAlpha = 0.0f;
}

void UEnclaveViewComponent::ApplySightAlignment()
{
	if (!bSightAlignmentBound)
	{
		return;
	}

	AEnclaveItem* Item = AlignmentSourceItem.Get();
	USkeletalMeshComponent* ItemMesh = IsValid(Item) ? Item->GetFirstPersonMesh() : nullptr;
	if (!IsValid(ViewCamera) || !IsValid(FirstPersonMesh) ||
		FirstPersonMesh->GetAttachParent() != ViewCamera ||
		!IsValid(ItemMesh) || ItemMesh->GetAttachParent() != FirstPersonMesh ||
		!ItemMesh->DoesSocketExist(ActiveSightSettings.SightSocketName))
	{
		// The item was un-equipped/destroyed, or the sight is no longer valid.
		// A stale cue must never leave the next item holding offset arms.
		RestoreSightAlignment();
		return;
	}

	if (CurrentSightAlignmentAlpha <= KINDA_SMALL_NUMBER)
	{
		FirstPersonMesh->SetRelativeTransform(BaseFirstPersonMeshRelativeTransform);
		return;
	}

	// Calibrate while the authored animation blends INTO aim (or for instant
	// aim). Once aimed, hold the calibrated root offset fixed: otherwise
	// tracking the animated sight every frame would cancel visible recoil and
	// authored weapon sway by moving the entire arms mesh in opposition.
	const bool bEnteringAim = bViewBlendInProgress && bFocusActive &&
		BlendTargetSightAlignmentAlpha > BlendStartSightAlignmentAlpha;
	if (!bHasSightCalibration || bEnteringAim)
	{
		// Weapon FP mesh -> hand socket -> arms mesh -> camera. Solve the
		// ARMS transform, not a weapon-only offset:
		// SightInArms * DesiredArmsInCamera = DesiredSightInCamera.
		const FTransform SightInArms = ItemMesh->GetSocketTransform(ActiveSightSettings.SightSocketName, RTS_World)
			.GetRelativeTransform(FirstPersonMesh->GetComponentTransform());
		const FTransform DesiredSightInCamera(
			ActiveSightSettings.SightCameraRotation.Quaternion(),
			ActiveSightSettings.SightCameraLocation,
			FVector::OneVector);
		CalibratedFirstPersonMeshRelativeTransform = SightInArms.Inverse() * DesiredSightInCamera;
		// ADS must never change the arms' authored scale.
		CalibratedFirstPersonMeshRelativeTransform.SetScale3D(BaseFirstPersonMeshRelativeTransform.GetScale3D());
		bHasSightCalibration = true;
	}

	FTransform BlendedArmsInCamera;
	BlendedArmsInCamera.Blend(BaseFirstPersonMeshRelativeTransform, CalibratedFirstPersonMeshRelativeTransform,
		FMath::Clamp(CurrentSightAlignmentAlpha, 0.0f, 1.0f));
	FirstPersonMesh->SetRelativeTransform(BlendedArmsInCamera);
}

void UEnclaveViewComponent::UpdateTickEnabled()
{
	// Keep a lightweight validity check during Focus. The calibrated ADS
	// offset stays fixed at full aim so recoil/sway animation is not negated.
	SetComponentTickEnabled(bViewBlendInProgress || bSightAlignmentBound);
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
	BlendStartSightAlignmentAlpha = CurrentSightAlignmentAlpha;
	BlendTargetSightAlignmentAlpha = bFocusActive && bSightAlignmentBound ? 1.0f : 0.0f;
	BlendDuration = Duration;
	BlendElapsed = 0.0f;
	bViewBlendInProgress = Duration > KINDA_SMALL_NUMBER;

	if (!bViewBlendInProgress)
	{
		ViewCamera->SetFieldOfView(BlendTargetFOV);
		CurrentSensitivityMultiplier = BlendTargetSensitivity;
		CurrentSightAlignmentAlpha = BlendTargetSightAlignmentAlpha;
		ApplySightAlignment();
		if (!bFocusActive)
		{
			bHasBaseFOV = false;
			RestoreSightAlignment();
		}
	}

	UpdateTickEnabled();
}

void UEnclaveViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsValid(ViewCamera) || !IsValid(CharacterOwner) || !CharacterOwner->IsLocallyControlled())
	{
		ResetFocus(/*bImmediate=*/true);
		return;
	}

	bool bCompletedBlendThisTick = false;
	if (bViewBlendInProgress)
	{
		BlendElapsed += DeltaTime;
		const float Alpha = FMath::Clamp(BlendElapsed / BlendDuration, 0.0f, 1.0f);
		ViewCamera->SetFieldOfView(FMath::Lerp(BlendStartFOV, BlendTargetFOV, Alpha));
		CurrentSensitivityMultiplier = FMath::Lerp(BlendStartSensitivity, BlendTargetSensitivity, Alpha);
		CurrentSightAlignmentAlpha = FMath::Lerp(BlendStartSightAlignmentAlpha, BlendTargetSightAlignmentAlpha, Alpha);

		if (Alpha >= 1.0f)
		{
			bCompletedBlendThisTick = true;
			if (!bFocusActive)
			{
				bHasBaseFOV = false;
			}
		}
	}

	// Sample the last entering frame before freezing the root calibration.
	ApplySightAlignment();
	if (bCompletedBlendThisTick)
	{
		bViewBlendInProgress = false;
	}
	if (!bFocusActive && !bViewBlendInProgress)
	{
		RestoreSightAlignment();
	}
	UpdateTickEnabled();
}
