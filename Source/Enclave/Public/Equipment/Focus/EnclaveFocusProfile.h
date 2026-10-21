// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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

	/** Seconds for the view and sensitivity transition in either direction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "0.0", Units = "s"))
	float TransitionDuration = 0.2f;

	/** Multiplies the normal locally configured look input while focused. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Focus|View", meta = (ClampMin = "0.01"))
	float LookSensitivityMultiplier = 0.65f;

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
