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
 * This class resolves the target's IEnclaveDamageReceiverInterface and does
 * not know whether that receiver is backed by simple health, GAS, armor,
 * shields, or another future implementation.
 */
UCLASS()
class ENCLAVE_API UEnclaveDamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Delivers DamageRequest to Target's damage receiver.
	 * Returns false when the request is invalid, Target is not authoritative,
	 * no receiver exists, or the receiver declines the request.
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Damage", meta = (DisplayName = "Apply Enclave Damage"))
	static bool ApplyDamage(AActor* Target, const FEnclaveDamageRequest& DamageRequest);
};
