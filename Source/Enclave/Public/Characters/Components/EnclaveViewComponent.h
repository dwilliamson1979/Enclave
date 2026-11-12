// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "Characters/Interfaces/EnclaveFocusViewInterface.h"
#include "Equipment/Focus/EnclaveFocusProfile.h"
#include "EnclaveViewComponent.generated.h"

class ACharacter;
class AEnclaveItem;
class UCameraComponent;
class USkeletalMeshComponent;

/**
 * Local view/look presentation. Input is only Look; item actions reach GAS
 * through Equipment. Focus arrives through IEnclaveFocusViewInterface
 * with resolved settings. This component never activates abilities.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveViewComponent : public UActorComponent, public IEnclaveInputHandlerInterface, public IEnclaveFocusViewInterface
{
	GENERATED_BODY()

public:
	UEnclaveViewComponent();

	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	/** Explicit camera binding; an avatar may have more than one camera. */
	void SetViewCamera(UCameraComponent* InCamera);

	/** Camera-attached arms; the weapon's first-person mesh stays attached to its hand socket. */
	void SetFirstPersonMesh(USkeletalMeshComponent* InMesh);

	// IEnclaveFocusViewInterface. Called with already-resolved item settings.
	virtual void BeginFocus(AEnclaveItem* SourceItem, const FEnclaveFocusViewSettings& Settings) override;
	virtual void EndFocus(AEnclaveItem* SourceItem, bool bSourcePresentationRemoved = false) override;

	/** Teardown/re-possession safety net; no stale Focus can retain camera changes. */
	void ResetFocus(bool bImmediate = false);

private:
	void RebuildInputFilters();
	bool HandleLookInput(const FEnclaveInputEvent& InputEvent);
	/** Starts moving toward bFocusActive's end from the current weight; instant when the duration is 0. */
	void StartFocusBlend();
	/** Moves Progress by DeltaTime and applies the eased weight to the FOV and look sensitivity. */
	void AdvanceFocus(float DeltaTime);
	/** Once fully out of Focus, forgets the base FOV and restores the arms. */
	void FinishFocusIfOut();
	bool IsFocusBlending() const { return bFocusActive ? FocusProgress < 1.0f : FocusProgress > 0.0f; }
	void ConfigureSightAlignment(AEnclaveItem* SourceItem, const FEnclaveFocusSightAlignmentSettings& Settings);
	/** bEnteringAim: this frame is part of the blend into Focus, so the sight calibration follows the animated pose. */
	void ApplySightAlignment(bool bEnteringAim);
	void RestoreSightAlignment();
	void UpdateTickEnabled();

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CharacterOwner;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input"))
	FGameplayTag LookInputTag;

	FGameplayTagContainer InputFilters;
	TWeakObjectPtr<AEnclaveItem> FocusSourceItem;

	// Retain the old sight source while blending OUT, even after the cue ends.
	// A weapon swap/detach instead restores the authored arms transform immediately.
	TWeakObjectPtr<AEnclaveItem> AlignmentSourceItem;
	FEnclaveFocusSightAlignmentSettings ActiveSightSettings;
	FTransform BaseFirstPersonMeshRelativeTransform = FTransform::Identity;
	FTransform CalibratedFirstPersonMeshRelativeTransform = FTransform::Identity;
	bool bSightAlignmentBound = false;
	bool bHasSightCalibration = false;

	// One transition drives FOV, look sensitivity and sight alignment (see
	// EnclaveFocusBlend). bFocusActive is the direction: true blends in.
	// ActiveViewSettings is the Focus being blended in or out; it is kept
	// after Focus ends so the way out uses the same FOV and easing.
	FEnclaveFocusViewSettings ActiveViewSettings;
	float FocusProgress = 0.0f;
	float FocusWeight = 0.0f;
	bool bFocusActive = false;
	bool bHasBaseFOV = false;
	float BaseFOV = 90.0f;
	float CurrentSensitivityMultiplier = 1.0f;
};
