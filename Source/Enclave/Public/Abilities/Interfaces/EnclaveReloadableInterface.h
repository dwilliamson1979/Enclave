// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveReloadableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveReloadableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * The full contract between UEnclaveGA_Reload and whatever triggered it -
 * same role IEnclaveFireableInterface plays for UEnclaveGA_Fire. The ability
 * owns the GAS lifecycle (activation gating, the Reloading state tag, the
 * reload cue, waiting out the duration); everything about *what* reloading
 * means for this particular thing lives behind these three methods.
 */
class ENCLAVE_API IEnclaveReloadableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Whether a reload would accomplish anything right now (magazine not
	 * already full; later, reserve ammo actually available). Checked from
	 * UEnclaveGA_Reload::CanActivateAbility on both the predicting client
	 * and the server - must only read replicated state.
	 */
	virtual bool CanReload() const = 0;

	/**
	 * Seconds from activation until the magazine is refilled. The
	 * implementer computes this rather than exposing a raw data-asset value
	 * so modifiers (a "hot-swap magazine" attachment, a perk) can scale it
	 * in one place without the ability knowing they exist.
	 */
	virtual float GetReloadDuration() const = 0;

	/** Server-only: the reload finished uninterrupted - move ammo into the magazine. Never called for a cancelled reload. */
	virtual void CompleteReload() = 0;
};
