// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Combat/EnclaveLagCompensationMode.h"
#include "EnclaveGameModeBase.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FEnclaveWeaponHitDetectionModeChanged, EEnclaveWeaponHitDetectionMode);

/**
 * Authoritative match rules for Enclave.
 */
UCLASS()
class ENCLAVE_API AEnclaveGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AEnclaveGameModeBase();

	UFUNCTION(BlueprintPure, Category = "Combat|Hit Detection")
	EEnclaveWeaponHitDetectionMode GetWeaponHitDetectionMode() const { return WeaponHitDetectionMode; }

    /** Effective server-only policy. SSR is not enabled until historical queries exist. */
    UFUNCTION(BlueprintPure, Category = "Combat|Lag Compensation")
    EEnclaveLagCompensationMode GetLagCompensationMode() const { return LagCompensationMode; }

    UFUNCTION(BlueprintPure, Category = "Combat|Lag Compensation")
    float GetMaxRewindAllowanceMs() const { return MaxRewindAllowanceMs; }

	/** Server-only notification; listeners configure their own collision representation. */
	FEnclaveWeaponHitDetectionModeChanged& OnWeaponHitDetectionModeChanged() { return WeaponHitDetectionModeChanged; }

	/** Authority-only match policy change. Locked after match start in Shipping/Test; development builds permit live testing. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hit Detection")
	void SetWeaponHitDetectionMode(EEnclaveWeaponHitDetectionMode NewMode);

	/** Development-only behavior: this Exec entry point does nothing in Test/Shipping. */
	UFUNCTION(Exec)
	void EnclaveSetHitRegistrationMode(const FString& ModeName);

protected:
	/**
	 * Reads optional server travel/startup overrides before any players spawn.
	 * Example: MapName?WeaponHitDetectionMode=SkeletalHitboxes
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

private:
	FEnclaveWeaponHitDetectionModeChanged WeaponHitDetectionModeChanged;

	/**
	 * Character representation used by authoritative Weapon traces for this match.
	 *
	 * Capsule is more forgiving and does not provide meaningful BoneName results.
	 * Skeletal Hitboxes uses the third-person mesh Physics Asset and supports
	 * bone-aware hit results. This is a match/server rule, not a weapon property.
	 *
	 * A dedicated server may override this before the match starts with the URL
	 * option ?WeaponHitDetectionMode=Capsule or =SkeletalHitboxes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Hit Detection", meta = (AllowPrivateAccess = "true"))
	EEnclaveWeaponHitDetectionMode WeaponHitDetectionMode = EEnclaveWeaponHitDetectionMode::Capsule;

    /** Match/server temporal policy, independent of collision representation.
     * Currently only Disabled is executable: selecting SSR logs a warning and forces Disabled at InitGame.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true"))
    EEnclaveLagCompensationMode LagCompensationMode = EEnclaveLagCompensationMode::Disabled;

    /** Future SSR historical-time cap, in milliseconds. Does not affect current traces. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "500.0", UIMin = "0.0", UIMax = "500.0", Units = "ms"))
    float MaxRewindAllowanceMs = 200.0f;
};
