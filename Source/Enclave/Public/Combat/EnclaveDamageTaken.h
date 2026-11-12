// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Combat/EnclaveDamageRequest.h"
#include "EnclaveDamageTaken.generated.h"

/**
 * What one damage request actually did to a receiver: the request, plus the
 * damage each pool really lost (no overkill, and after armor once it exists).
 *
 * Produced on the server by the receiver when the request changed something,
 * before the receiver announces depletion, so a killing blow is always seen.
 * ShieldDamage is added when shields exist.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveDamageTaken
{
	GENERATED_BODY()

	FEnclaveDamageTaken() = default;

	FEnclaveDamageTaken(const FEnclaveDamageRequest& InRequest, float InHealthDamage)
		: Request(InRequest)
		, HealthDamage(InHealthDamage)
	{
	}

	/** The request that caused the damage. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	FEnclaveDamageRequest Request;

	/** Health actually lost: positive, never more than the health the receiver had. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	float HealthDamage = 0.0f;
};
