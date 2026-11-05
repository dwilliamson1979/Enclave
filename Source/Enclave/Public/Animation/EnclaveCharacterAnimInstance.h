// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/EnclaveGameplayTagPropertyMap.h"
#include "Animation/Interfaces/EnclaveAnimDataProviderInterface.h"
#include "EnclaveCharacterAnimInstance.generated.h"

class APawn;
class UAbilitySystemComponent;

/**
 * Shared native parent for every Enclave avatar Animation Blueprint - both
 * the main ABPs assigned to a mesh (ABP_Enclave_FP / ABP_Enclave_TP) and
 * the per-item layer ABPs linked into them (ABP_FP_RifleLayers, ...).
 *
 * Two inputs, two update phases:
 *
 * - Game thread (NativeUpdateAnimation): copy a snapshot from the pawn via
 *   IEnclaveAnimDataProviderInterface, and keep GameplayTagBindings bound to
 *   the pawn's ASC. This is the only place UObjects are touched.
 * - Worker thread (NativeThreadSafeUpdateAnimation): derive GroundSpeed,
 *   MoveDirection and bIsMoving from that snapshot. Touches only this
 *   instance's own members.
 *
 * Linked layer instances are separate UAnimInstances. Because they share
 * this parent, each one resolves its own data and binds its own tags,
 * so a layer ABP never needs to know which main ABP it's linked into.
 */
UCLASS()
class ENCLAVE_API UEnclaveCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/** Unbinds the tag bindings: the ASC (on the PlayerState) outlives this instance's pawn. */
	virtual void NativeUninitializeAnimation() override;

#if WITH_EDITOR
	/** Flags GameplayTagBindings entries with no tag, or a missing or wrongly typed property. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/**
	 * Gameplay Tag -> ABP variable bindings, configured per ABP in Class
	 * Defaults (e.g. State.Item.Aiming -> bIsAiming). Driven by the ASC's
	 * tag-change events, not polled. Supported property types: bool (tag
	 * present), int, float or double (tag count).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Enclave|Gameplay Tags")
	FEnclaveGameplayTagPropertyMap GameplayTagBindings;

	/** This frame's snapshot from the pawn. Default-constructed when no provider exists. */
	UPROPERTY(BlueprintReadOnly, Category = "Enclave|Anim Data")
	FEnclaveAnimData AnimData;

	/** Horizontal speed, cm/s. Derived on the worker thread. */
	UPROPERTY(BlueprintReadOnly, Category = "Enclave|Locomotion")
	float GroundSpeed = 0.0f;

	/** Direction of travel relative to facing, -180..180 (0 = forward, 90 = right). Derived on the worker thread. */
	UPROPERTY(BlueprintReadOnly, Category = "Enclave|Locomotion")
	float MoveDirection = 0.0f;

	/** GroundSpeed above MoveSpeedThreshold. Derived on the worker thread. */
	UPROPERTY(BlueprintReadOnly, Category = "Enclave|Locomotion")
	bool bIsMoving = false;

	/** Speed (cm/s) above which the avatar counts as moving. */
	UPROPERTY(EditDefaultsOnly, Category = "Enclave|Locomotion")
	float MoveSpeedThreshold = 3.0f;

private:
	/** Re-resolves the provider only when the owning pawn changes. */
	bool TryRefreshPawnBinding();

	/**
	 * Keeps GameplayTagBindings bound to the pawn's current ASC. The ASC lives
	 * on the PlayerState and is only reachable after possession (server) /
	 * PlayerState replication (clients), which can land after this instance
	 * initializes, and a new PlayerState brings a new ASC (seamless travel).
	 * So this checks each frame and rebinds only when the ASC differs.
	 */
	void TryBindGameplayTagBindings(APawn* Pawn);

	void ResetAnimationState();

	TWeakObjectPtr<APawn> CachedPawn;

	/** Only valid while CachedPawn is valid - always check CachedPawn first. */
	const IEnclaveAnimDataProviderInterface* AnimDataProvider = nullptr;
};
