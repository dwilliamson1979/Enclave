// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnclaveFocusProfile.generated.h"

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
