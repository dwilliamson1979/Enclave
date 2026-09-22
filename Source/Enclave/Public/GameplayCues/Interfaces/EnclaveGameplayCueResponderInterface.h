// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "EnclaveGameplayCueResponderInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveGameplayCueResponderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implemented by whatever a Gameplay Cue notify should hand its tag and
 * parameters to, so it can decide for itself how (or whether) to respond.
 *
 * One stable method, not one method per cue: a Gameplay Cue Notify still
 * needs one registered notify class per distinct tag (the Gameplay Cue
 * Manager dispatches by exact tag match, with no built-in wildcard/parent-
 * tag routing), so every notify already knows exactly which tag it's for -
 * this interface doesn't need to encode that in its own signature. Keeping
 * the interface to a single method also means adding a new cue never forces
 * an unrelated implementer (a future non-weapon actor implementing this same
 * interface for a different subset of cues) to add an override it doesn't
 * care about - that decision lives entirely inside each implementation's own
 * dispatch instead.
 *
 * Same shape as IEnclaveHitFeedbackInterface/IEnclaveWieldableInterface
 * elsewhere in the project: depend on the interface, find the implementer
 * with FindComponentByInterface or Cast, never reference a concrete class
 * across domains.
 */
class ENCLAVE_API IEnclaveGameplayCueResponderInterface
{
	GENERATED_BODY()

public:
	/**
	 * A Gameplay Cue notify's OnExecute_Implementation forwards straight to
	 * this - CueTag is that notify's own GameplayCueTag (or
	 * Parameters.OriginalTag, which is the same value), Parameters is
	 * whatever the triggering ability populated. Returns true if this
	 * implementation recognized and handled the tag, false otherwise -
	 * implementations should log on an unhandled tag reaching them rather
	 * than silently returning false, so a missing branch after adding a new
	 * cue shows up immediately instead of as a silent no-op.
	 */
	virtual bool HandleGameplayCue(FGameplayTag CueTag, const FGameplayCueParameters& Parameters) = 0;
};
