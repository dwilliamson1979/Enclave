// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"
#include "Combat/EnclaveShotTiming.h"
#include "Combat/EnclaveSSRGameplayClaim.h"
#include "Debug/EnclaveDebug.h"
#include "GameFramework/Actor.h"
#include "UObject/CoreNet.h"
#include "EnclaveShotIntent.generated.h"

/** UNTRUSTED shooter viewpoint captured on the owning client. The server
 * must validate it before it can influence authoritative hit registration.
 * The timestamp is NOT yet suitable for historical target rewind.
 */
USTRUCT()
struct ENCLAVE_API FEnclaveShotIntent
{
	GENERATED_BODY()

	UPROPERTY()
	FVector_NetQuantize10 Origin = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	/** Shooter's estimate of synchronized server time (double for long sessions).
	 * This is untrusted timing evidence, NOT an authoritative rewind timestamp.
	 */
	UPROPERTY()
	double ClientServerTimeSeconds = 0.0;

	/** Diagnostics/ordering. Each shot uses its own GAS activation prediction key. */
	UPROPERTY()
	uint16 ShotSequence = 0;

	/** Reserved for a future server-enforced spread policy. Always zero for now. */
	UPROPERTY()
	uint32 SpreadSeed = 0;
	/** Development-only shadow test. Never hit authority. */
	FEnclaveSSRGameplayClaim SSRGameplayClaim;
};

/** Server-accepted ray, created only after validating an untrusted shot intent.
 * This is an in-process value: never send it over the network as client authority.
 * Rewind will later require more accurate time sync and historical shooter poses.
 */
struct ENCLAVE_API FEnclaveValidatedShotRay
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector; // normalized
	double ClientServerTimeSeconds = 0.0; // provenance, NOT a rewind timestamp

	/** Server-owned receipt time and future historical-lookup policy. Never authorizes hits on its own. */
	FEnclaveShotTimingContext Timing;

	/** Correlation only: the validated client shot key, never hit authority. */
	uint16 ShotSequence = 0;
	FEnclaveSSRGameplayClaim SSRGameplayClaim; // shadow-only
};

/** GAS Target Data envelope. Must use its concrete ScriptStruct for polymorphic network serialization. */
USTRUCT()
struct ENCLAVE_API FEnclaveGameplayAbilityTargetData_ShotIntent : public FGameplayAbilityTargetData
{
	GENERATED_BODY()

	UPROPERTY()
	FEnclaveShotIntent Shot;

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }
	virtual FString ToString() const override { return TEXT("EnclaveShotIntent"); }

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bool bOriginOk = true;
		bool bDirectionOk = true;
		Shot.Origin.NetSerialize(Ar, Map, bOriginOk);
		Shot.Direction.NetSerialize(Ar, Map, bDirectionOk);
		Ar << Shot.ClientServerTimeSeconds;
		Ar << Shot.ShotSequence;
		Ar << Shot.SpreadSeed;
#if ENCLAVE_INCLUDE_DEBUG_CODE
		// Same GAS TargetData RPC: no second packet stream or additional shot RPC.
		uint8 HasSample = Shot.SSRGameplayClaim.Classification != 0 ? 1 : 0;
		Ar.SerializeBits(&HasSample, 1);
		if (HasSample)
		{
			Ar << Shot.SSRGameplayClaim.Classification;
			UObject* TargetObject = Shot.SSRGameplayClaim.Target.Get();
			const bool bMapped = Map && Map->SerializeObject(Ar, AActor::StaticClass(), TargetObject);
			if (Ar.IsLoading())
			{
				Shot.SSRGameplayClaim.Target = Cast<AActor>(TargetObject);
			}
			Ar << Shot.SSRGameplayClaim.EchoedMovementStamp;
			Ar << Shot.SSRGameplayClaim.SecondsSinceMovementUpdate;
			// Mapping failure disables only the optional diagnostic claim.
			// Never reject an otherwise legal GAS shot because a test target
			// actor was not resolved by the connection's package map.
			if (!bMapped)
			{
				Shot.SSRGameplayClaim = FEnclaveSSRGameplayClaim();
			}
		}
#endif
		bOutSuccess = bOriginOk && bDirectionOk && !Ar.IsError();
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FEnclaveGameplayAbilityTargetData_ShotIntent> : TStructOpsTypeTraitsBase2<FEnclaveGameplayAbilityTargetData_ShotIntent>
{
	enum { WithNetSerializer = true, WithCopy = true };
};
