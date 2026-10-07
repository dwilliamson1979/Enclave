// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "EnclaveDamageRequest.generated.h"

class AActor;

/**
 * Storage- and implementation-agnostic description of one damage attempt.
 *
 * Damage is always expressed as a positive amount. The request carries the
 * generic combat context a receiver may need, but deliberately contains no
 * GAS types, health-component types, or other concrete damage implementation
 * details. A simple prop may only care about Damage; a future combatant
 * receiver may inspect SourceActor, DamageCauser, and HitResult as part of
 * armor, shield, resistance, or hit-location resolution.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveDamageRequest
{
	GENERATED_BODY()

	FEnclaveDamageRequest() = default;

	FEnclaveDamageRequest(float InDamage, AActor* InSourceActor, AActor* InDamageCauser, const FHitResult& InHitResult)
		: Damage(InDamage)
		, SourceActor(InSourceActor)
		, DamageCauser(InDamageCauser)
		, HitResult(InHitResult)
	{
	}

	/** Positive amount of damage being requested. Zero/negative values are invalid damage requests. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Damage", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	/** Actor ultimately responsible for the damage, such as the firing avatar. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat|Damage")
	TObjectPtr<AActor> SourceActor = nullptr;

	/** Actor or object actor that physically caused the damage, such as a weapon or projectile. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat|Damage")
	TObjectPtr<AActor> DamageCauser = nullptr;

	/** Optional spatial/contact context for hit-based damage. May be empty for non-hit damage such as hazards. */
	UPROPERTY(BlueprintReadWrite, Category = "Combat|Damage")
	FHitResult HitResult;
};
