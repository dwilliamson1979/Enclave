// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/NetSerialization.h"
#include "EnclaveShotIntent.generated.h"

/** UNTRUSTED captured shooter viewpoint. At Stage A it is transport-only;
 * authoritative traces still use the server viewpoint. Do not treat this
 * payload as validated shot input until Stage B.
 */
USTRUCT()
struct ENCLAVE_API FEnclaveShotIntent
{
    GENERATED_BODY()

    UPROPERTY()
    FVector_NetQuantize10 Origin = FVector::ZeroVector;

    UPROPERTY()
    FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

    /** Shooter's estimate of synchronized server time, NOT trusted rewind time. */
    UPROPERTY()
    float ClientServerTimeSeconds = 0.0f;

    /** Diagnostics/ordering. Each shot uses its own GAS activation prediction key. */
    UPROPERTY()
    uint16 ShotSequence = 0;

    /** Reserved for a future server-enforced spread policy. Always zero for now. */
    UPROPERTY()
    uint32 SpreadSeed = 0;
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
        bOutSuccess = bOriginOk && bDirectionOk && !Ar.IsError();
        return true;
    }
};

template<>
struct TStructOpsTypeTraits<FEnclaveGameplayAbilityTargetData_ShotIntent> : TStructOpsTypeTraitsBase2<FEnclaveGameplayAbilityTargetData_ShotIntent>
{
    enum { WithNetSerializer = true, WithCopy = true };
};
