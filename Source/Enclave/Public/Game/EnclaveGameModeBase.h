// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "EnclaveGameModeBase.generated.h"

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

protected:
	/**
	 * Reads optional server travel/startup overrides before any players spawn.
	 * Example: MapName?WeaponHitDetectionMode=SkeletalHitboxes
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

private:
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
};
