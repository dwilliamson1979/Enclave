// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Kismet/KismetMathLibrary.h"
#include "EnclaveFocusProfile.generated.h"

/**
 * Physical sight calibration for the owning player's camera-attached arms.
 * Authored on the Focus profile (and therefore replaceable by a future optic).
 * No gameplay fire origin, camera transform, or third-person mesh is changed.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveFocusSightAlignmentSettings
{
	GENERATED_BODY()

	/** Opt in per item; older profiles retain their FOV-only presentation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|Sight")
	bool bEnabled = false;

	/** Socket on the item's FIRST-PERSON mesh, located at its rear sight/eyepiece. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|Sight")
	FName SightSocketName = TEXT("ADS");

	/** Where the sight socket should end up relative to the camera, in centimeters (X = forward). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|Sight", meta = (Units = "cm"))
	FVector SightCameraLocation = FVector(12.0f, 0.0f, 0.0f);

	/** Socket's desired orientation relative to the camera (socket +X points down the bore). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|Sight")
	FRotator SightCameraRotation = FRotator::ZeroRotator;
};

/** Local visual settings; no equipment or gameplay-ability policy is stored here. */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveFocusViewSettings
{
	GENERATED_BODY()

	/** Absolute camera Field of View (degrees) while focused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "5.0", ClampMax = "170.0", Units = "deg"))
	float FocusFOV = 65.0f;

	/**
	 * Seconds for a full transition into Focus (FOV, sensitivity and sight
	 * alignment move together). 0 = instant. Was TransitionDuration.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float EnterDuration = 0.2f;

	/**
	 * Seconds for a full transition out of Focus. 0 = instant. A reversal part
	 * way covers only the distance already travelled: release at half way and
	 * the way out takes half of this.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ExitDuration = 0.2f;

	/**
	 * Shape of the transition into Focus. The way out mirrors it in time, so
	 * both directions feel the same: EaseOut (or ExpoOut, CircularOut) starts
	 * fast and settles, the feel of an interp-speed zoom, both in and out.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View")
	TEnumAsByte<EEasingFunc::Type> TransitionEasing = EEasingFunc::Linear;

	/** Strength of EaseIn, EaseOut and EaseInOut (2 = quadratic); the other shapes ignore it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "1.0"))
	float TransitionEasingExponent = 2.0f;

	/**
	 * Look sensitivity already follows the zoom: it is scaled by
	 * tan(FOV / 2) / tan(hip FOV / 2), so the view turns at the same on-screen
	 * speed at any magnification and hip sensitivity is never changed. This
	 * multiplies on top of that while focused (1 = pure zoom matching; below 1
	 * slower). Replaces LookSensitivityMultiplier, which was the whole scale
	 * (0.65 then is about 1.0 now), so it is deliberately not redirected.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "0.01"))
	float LookSensitivityAdjustment = 1.0f;

	/** Optional camera-space alignment of the physical weapon sights. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|Sight")
	FEnclaveFocusSightAlignmentSettings SightAlignment;
};

/**
 * Optional, reusable Focus presentation asset. Deliberately not part of
 * UEnclaveItemDefinition: only items implementing the Focus provider refer to it.
 * Later an optic may provide a different asset at runtime.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveFocusProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	const FEnclaveFocusViewSettings& GetViewSettings() const { return ViewSettings; }

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus", meta = (AllowPrivateAccess = "true"))
	FEnclaveFocusViewSettings ViewSettings;
};
