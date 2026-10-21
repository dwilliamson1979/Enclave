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
	void BeginViewBlend(float InFOV, float InSensitivity, float Duration);
	void ConfigureSightAlignment(AEnclaveItem* SourceItem, const FEnclaveFocusSightAlignmentSettings& Settings);
	void ApplySightAlignment();
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
	float CurrentSightAlignmentAlpha = 0.0f;
	float BlendStartSightAlignmentAlpha = 0.0f;
	float BlendTargetSightAlignmentAlpha = 0.0f;

	bool bViewBlendInProgress = false;
	bool bFocusActive = false;
	bool bHasBaseFOV = false;
	float BaseFOV = 90.0f;
	float FocusTransitionDuration = 0.2f;
	float CurrentSensitivityMultiplier = 1.0f;
	float BlendStartSensitivity = 1.0f;
	float BlendTargetSensitivity = 1.0f;
	float BlendStartFOV = 90.0f;
	float BlendTargetFOV = 90.0f;
	float BlendDuration = 0.0f;
	float BlendElapsed = 0.0f;
};
