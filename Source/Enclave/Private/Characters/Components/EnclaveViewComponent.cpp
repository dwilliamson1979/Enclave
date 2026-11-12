// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveViewComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/EnclaveDebug.h"
#include "GameFramework/Character.h"
#include "Equipment/Focus/EnclaveFocusBlend.h"
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

	const float CurrentFOV = ViewCamera->FieldOfView;
	ActiveViewSettings = Settings;
	ActiveViewSettings.FocusFOV = FMath::Clamp(Settings.FocusFOV, 5.0f, 170.0f);
	ActiveViewSettings.LookSensitivityMultiplier = FMath::Max(0.01f, Settings.LookSensitivityMultiplier);
	ActiveViewSettings.EnterDuration = FMath::Max(0.0f, Settings.EnterDuration);
	ActiveViewSettings.ExitDuration = FMath::Max(0.0f, Settings.ExitDuration);

	// Still partly zoomed (a quick re-press, or another item while the last
	// one blends out): start from the weight that gives the current FOV under
	// the new settings, so the FOV doesn't jump. For the same item it's the
	// same weight. A new item's sight alignment does start at this weight,
	// so its arms move part way at once; rare (a swap is slower than the
	// blend-out) and accepted.
	if (FocusWeight > 0.0f && !FMath::IsNearlyEqual(ActiveViewSettings.FocusFOV, BaseFOV))
	{
		FocusWeight = FMath::Clamp((CurrentFOV - BaseFOV) / (ActiveViewSettings.FocusFOV - BaseFOV), 0.0f, 1.0f);
	}

	bFocusActive = true;
	FocusSourceItem = SourceItem;
	ConfigureSightAlignment(SourceItem, ActiveViewSettings.SightAlignment);
	// Entering maps Progress to Weight differently from leaving: re-solve so a
	// reversal continues from the current view and takes only the time spent.
	FocusProgress = EnclaveFocusBlend::ProgressForWeight(FocusWeight, /*bEntering=*/true, ActiveViewSettings);
	StartFocusBlend();
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
		FocusProgress = 0.0f;
		FocusWeight = 0.0f;
		RestoreSightAlignment();
		UpdateTickEnabled();
		return;
	}

	FocusProgress = EnclaveFocusBlend::ProgressForWeight(FocusWeight, /*bEntering=*/false, ActiveViewSettings);
	StartFocusBlend();
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
}

void UEnclaveViewComponent::ApplySightAlignment(bool bEnteringAim)
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

	if (FocusWeight <= KINDA_SMALL_NUMBER)
	{
		FirstPersonMesh->SetRelativeTransform(BaseFirstPersonMeshRelativeTransform);
		return;
	}

	// Calibrate while the authored animation blends INTO aim (or for instant
	// aim). Once aimed, hold the calibrated root offset fixed: otherwise
	// tracking the animated sight every frame would cancel visible recoil and
	// authored weapon sway by moving the entire arms mesh in opposition.
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
		FMath::Clamp(FocusWeight, 0.0f, 1.0f));
	FirstPersonMesh->SetRelativeTransform(BlendedArmsInCamera);
}

void UEnclaveViewComponent::UpdateTickEnabled()
{
	// Keep a lightweight validity check during Focus. The calibrated ADS
	// offset stays fixed at full aim so recoil/sway animation is not negated.
	SetComponentTickEnabled(IsFocusBlending() || bSightAlignmentBound);
}

void UEnclaveViewComponent::StartFocusBlend()
{
	if (!IsValid(ViewCamera))
	{
		return;
	}

	const float Duration = bFocusActive ? ActiveViewSettings.EnterDuration : ActiveViewSettings.ExitDuration;
	if (Duration <= KINDA_SMALL_NUMBER)
	{
		// Instant: arrive now rather than on the next tick.
		AdvanceFocus(0.0f);
		ApplySightAlignment(/*bEnteringAim=*/false);
		FinishFocusIfOut();
	}

	UpdateTickEnabled();
}

void UEnclaveViewComponent::AdvanceFocus(float DeltaTime)
{
	const float Target = bFocusActive ? 1.0f : 0.0f;
	const float Duration = bFocusActive ? ActiveViewSettings.EnterDuration : ActiveViewSettings.ExitDuration;
	FocusProgress = Duration > KINDA_SMALL_NUMBER
		? FMath::FInterpConstantTo(FocusProgress, Target, DeltaTime, 1.0f / Duration)
		: Target;
	FocusWeight = EnclaveFocusBlend::Weight(FocusProgress, bFocusActive, ActiveViewSettings);

	ViewCamera->SetFieldOfView(FMath::Lerp(BaseFOV, ActiveViewSettings.FocusFOV, FocusWeight));
	CurrentSensitivityMultiplier = FMath::Lerp(1.0f, ActiveViewSettings.LookSensitivityMultiplier, FocusWeight);
}

void UEnclaveViewComponent::FinishFocusIfOut()
{
	if (!bFocusActive && FocusProgress <= 0.0f)
	{
		bHasBaseFOV = false;
		RestoreSightAlignment();
	}
}

void UEnclaveViewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsValid(ViewCamera) || !IsValid(CharacterOwner) || !CharacterOwner->IsLocallyControlled())
	{
		ResetFocus(/*bImmediate=*/true);
		return;
	}

	// Read before advancing, so the frame that arrives at full aim still
	// samples the animated sight before the calibration is frozen.
	const bool bEnteringAim = bFocusActive && FocusProgress < 1.0f;
	if (IsFocusBlending())
	{
		AdvanceFocus(DeltaTime);
	}

	ApplySightAlignment(bEnteringAim);
	FinishFocusIfOut();
	UpdateTickEnabled();
}
