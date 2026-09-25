// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayEffectTypes.h"
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
 *   IEnclaveAnimDataProviderInterface, and bind GameplayTagPropertyMap to
 *   the pawn's ASC once one exists. This is the only place UObjects are
 *   touched.
 * - Worker thread (NativeThreadSafeUpdateAnimation): derive GroundSpeed,
 *   MoveDirection and bIsMoving from that snapshot. Touches only this
 *   instance's own members.
 *
 * Linked layer instances are separate UAnimInstances. Because they share
 * this parent, each one resolves its own data and binds its own tag map,
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

#if WITH_EDITOR
	/** Flags GameplayTagPropertyMap entries that point at missing or wrongly typed properties. */
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

protected:
	/**
	 * Gameplay Tag -> ABP variable bindings, configured per ABP in Class
	 * Defaults (e.g. State.Weapon.Reloading -> bIsReloading). Driven by the
	 * ASC's tag-change events, not polled. Supported property types: bool
	 * (tag present), int (tag count), float (tag count).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Enclave|Gameplay Tags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;

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
	 * Binds GameplayTagPropertyMap the first time the pawn's ASC is
	 * available. The ASC lives on the PlayerState and is only reachable
	 * after possession (server) / PlayerState replication (clients), which
	 * can land after this instance initializes - so this retries each frame
	 * until it succeeds, then no-ops.
	 */
	void TryBindGameplayTagPropertyMap(APawn* Pawn);

	void ResetAnimationState();

	TWeakObjectPtr<APawn> CachedPawn;

	/** Only valid while CachedPawn is valid - always check CachedPawn first. */
	const IEnclaveAnimDataProviderInterface* AnimDataProvider = nullptr;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
};
