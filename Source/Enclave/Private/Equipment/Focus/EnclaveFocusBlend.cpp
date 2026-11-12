// Copyright Lightcarrier Studios

#include "Equipment/Focus/EnclaveFocusBlend.h"

#include "Equipment/Focus/EnclaveFocusProfile.h"
#include "Kismet/KismetMathLibrary.h"

namespace
{
	float Ease01(float Alpha, const FEnclaveFocusViewSettings& Settings)
	{
		return static_cast<float>(UKismetMathLibrary::Ease(0.0, 1.0, FMath::Clamp(Alpha, 0.0f, 1.0f),
			Settings.TransitionEasing, Settings.TransitionEasingExponent));
	}
}

float EnclaveFocusBlend::Weight(float Progress, bool bEntering, const FEnclaveFocusViewSettings& Settings)
{
	const float P = FMath::Clamp(Progress, 0.0f, 1.0f);
	return bEntering ? Ease01(P, Settings) : 1.0f - Ease01(1.0f - P, Settings);
}

float EnclaveFocusBlend::ProgressForWeight(float InWeight, bool bEntering, const FEnclaveFocusViewSettings& Settings)
{
	const float Target = FMath::Clamp(InWeight, 0.0f, 1.0f);
	if (Target <= 0.0f || Target >= 1.0f)
	{
		return Target;
	}

	// Weight rises with Progress in both directions, so bisect. 20 halvings
	// is about 1e-6, far below a visible FOV step; this runs only when Focus
	// starts or ends, not every frame.
	float Low = 0.0f;
	float High = 1.0f;
	for (int32 Iteration = 0; Iteration < 20; ++Iteration)
	{
		const float Mid = 0.5f * (Low + High);
		if (Weight(Mid, bEntering, Settings) < Target)
		{
			Low = Mid;
		}
		else
		{
			High = Mid;
		}
	}
	return 0.5f * (Low + High);
}
