// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/EnclaveDamageRequest.h"
#include "EnclaveDamageStatics.generated.h"

class AActor;

/**
 * Stable external entry point for Enclave damage delivery.
 *
 * Callers provide a target and an implementation-agnostic damage request.
 * This class asks the game mode's rules (IEnclaveDamagePolicyInterface) what
 * the hit does, then resolves the target's IEnclaveDamageReceiverInterface. It
 * does not know whether that receiver is backed by simple health, GAS, armor,
 * shields, or another future implementation.
 */
UCLASS()
class ENCLAVE_API UEnclaveDamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Server only. Delivers DamageRequest to Target's damage receiver, as the
	 * game mode's damage policy decides (a game mode without one applies it):
	 * Apply delivers the (possibly changed) request, Simulate returns true
	 * without changing health (the hit still shows a hit marker), Ignore
	 * returns false. Also returns false when the request is invalid, Target is
	 * not authoritative, no receiver exists, or the receiver declines it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage", meta = (DisplayName = "Apply Enclave Damage"))
	static bool ApplyDamage(AActor* Target, const FEnclaveDamageRequest& DamageRequest);
};
