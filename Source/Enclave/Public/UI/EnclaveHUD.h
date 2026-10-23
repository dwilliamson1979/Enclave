// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "EnclaveHUD.generated.h"

/**
 * Minimal local-player HUD used while the full Enclave UI is still in development.
 * Draws a fixed, centered reticle. It is presentation-only: firing, hit traces,
 * item state, GAS abilities, and SSR do not depend on this class.
 */
UCLASS()
class ENCLAVE_API AEnclaveHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
