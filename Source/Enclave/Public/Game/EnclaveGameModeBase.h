// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Combat/EnclaveLagCompensationMode.h"
#include "Combat/EnclaveSSRPolicyContracts.h"
#include "Combat/EnclaveSSRTimingPolicy.h"
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

	/** Effective server-only policy. */
	UFUNCTION(BlueprintPure, Category = "Combat|Lag Compensation")
	EEnclaveLagCompensationMode GetLagCompensationMode() const { return LagCompensationMode; }

	/** True when validated remote shots are registered against history (Capsule mode only). */
	bool IsServerSideRewindActive() const
	{
		return LagCompensationMode == EEnclaveLagCompensationMode::ServerSideRewind &&
			WeaponHitDetectionMode == EEnclaveWeaponHitDetectionMode::Capsule;
	}

	/** Server-only timing policy inputs for ResolveSSRTiming(). */
	FEnclaveSSRTimingSettings GetSSRTimingSettings() const;

	EEnclaveSSRUnavailableFallbackPolicy GetSSRUnavailableFallbackPolicy() const { return SSRUnavailableFallbackPolicy; }

	/** Whether characters must replicate history stamps for clients to echo. */
	bool RequiresClientSSRStamps() const
	{
		return IsServerSideRewindActive() && SSRTimingSource != EEnclaveSSRTimingSource::ServerAnchor;
	}

	UFUNCTION(BlueprintPure, Category = "Combat|Lag Compensation")
	float GetMaxRewindAllowanceMs() const { return MaxRewindAllowanceMs; }

	/** Single authority for the effective bounded rewind window in seconds. */
	double GetMaxRewindSeconds() const
	{
		return FMath::Clamp(static_cast<double>(MaxRewindAllowanceMs), 0.0, 500.0) / 1000.0;
	}

	/** Server-only notification; listeners configure their own collision representation. */
	FEnclaveWeaponHitDetectionModeChanged& OnWeaponHitDetectionModeChanged() { return WeaponHitDetectionModeChanged; }

	/** Authority-only match policy change. Locked after match start in Shipping/Test; development builds permit live testing. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Hit Detection")
	void SetWeaponHitDetectionMode(EEnclaveWeaponHitDetectionMode NewMode);

	/** Development-only behavior: this Exec entry point does nothing in Test/Shipping. */
	UFUNCTION(Exec)
	void EnclaveSetHitRegistrationMode(const FString& ModeName);

	/** Development-only: switch SSR timing source live (ServerAnchor|EchoedStamp|Hybrid). */
	UFUNCTION(Exec)
	void EnclaveSetSSRTimingSource(const FString& SourceName);

protected:
	/**
	 * Reads optional server travel/startup overrides before any players spawn.
	 * Example: MapName?WeaponHitDetectionMode=SkeletalHitboxes
	 * SSR: ?LagCompensation=ServerSideRewind ?SSRTimingSource=Hybrid|ServerAnchor|EchoedStamp
	 */
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

private:
	/** Reads ?Key=EnumValueName from server options; invalid names are logged and ignored. */
	template <typename TEnum>
	static void ParseEnumOption(const FString& Options, const TCHAR* Key, TEnum& InOutValue);

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
	 * ServerSideRewind only applies in Capsule mode; SkeletalHitboxes stays present-time.
	 * Override with the URL option ?LagCompensation=ServerSideRewind|Disabled.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true"))
	EEnclaveLagCompensationMode LagCompensationMode = EEnclaveLagCompensationMode::Disabled;

	/** How the rewind time is chosen. Hybrid is the supported default; the other
	 * two are fallback/diagnostic modes. URL option ?SSRTimingSource=.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true"))
	EEnclaveSSRTimingSource SSRTimingSource = EEnclaveSSRTimingSource::Hybrid;

	/** Signed offset from the server RTT anchor: target age = RTT + offset.
	 * -25 ms was as good as or better than -50 ms in every dedicated-server condition
	 * measured in milestone 3 (Documents/SSR_M3_NetworkValidation.md); the earlier
	 * -50 ms came from a listen server, whose host frame time inflates ping.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true", ClampMin = "-150.0", ClampMax = "100.0", Units = "ms"))
	float SSRAnchorOffsetMs = -25.0f;

	/** Hybrid: an echoed stamp may be this much younger than the anchor age. Bounds backtracking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0", Units = "ms"))
	float SSRStampEarlyToleranceMs = 40.0f;

	/** Hybrid: an echoed stamp may be this much older than the anchor age. Bounds backtracking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "100.0", Units = "ms"))
	float SSRStampLateToleranceMs = 60.0f;

	/** What a valid shot does when no trustworthy historical answer exists.
	 * Never applies to a definitive historical miss or a world-occluded hit.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true"))
	EEnclaveSSRUnavailableFallbackPolicy SSRUnavailableFallbackPolicy = EEnclaveSSRUnavailableFallbackPolicy::PresentTimeTrace;

	/** Historical-time cap, in milliseconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "500.0", UIMin = "0.0", UIMax = "500.0", Units = "ms"))
	float MaxRewindAllowanceMs = 200.0f;
};
