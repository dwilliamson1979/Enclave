// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "EnclaveAvatarPresentationComponent.generated.h"

class AEnclaveItem;
class UEnclaveItemPresentationDefinition;
class USkeletalMeshComponent;
class UAnimInstance;
class UAnimMontage;
struct FEnclaveItemPresentationMontages;

/**
 * Runtime record of the persistent wielded-item presentation this component
 * actually applied. Teardown uses this state rather than querying the gameplay
 * item again, because that item may already have been destroyed or become stale.
 */
USTRUCT()
struct ENCLAVE_API FEnclavePresentedWieldedItemState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<AEnclaveItem> Item;

	UPROPERTY(Transient)
	FGameplayTag ItemType;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> FirstPersonAnimLayers;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> ThirdPersonAnimLayers;

	UPROPERTY(Transient)
	bool bWieldedAttachmentPending = false;

	UPROPERTY(Transient)
	bool bIsActive = false;

	bool IsActive() const { return bIsActive; }

	void Reset()
	{
		Item.Reset();
		ItemType = FGameplayTag();
		FirstPersonAnimLayers = nullptr;
		ThirdPersonAnimLayers = nullptr;
		bWieldedAttachmentPending = false;
		bIsActive = false;
	}
};

/**
 * Coordinates how gameplay state and semantic gameplay events are presented
 * by this avatar.
 *
 * This component owns presentation behavior only. It does not own authoritative
 * gameplay state and must not determine whether gameplay actions such as
 * equipping, firing, reloading, or taking damage are allowed to occur.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveAvatarPresentationComponent : public UActorComponent, public IEnclaveAvatarPresentationInterface
{
	GENERATED_BODY()

public:
	UEnclaveAvatarPresentationComponent();

	void SetPresentationMeshes(USkeletalMeshComponent* InFirstPersonMesh, USkeletalMeshComponent* InThirdPersonMesh);

	// IEnclaveAvatarPresentationInterface
	virtual void PresentWieldedItem(AEnclaveItem* Item) override;
	virtual void ClearWieldedItemPresentation() override;
	virtual void HandleItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event) override;
	virtual void HandleItemPresentationChoreography(const FGameplayTag& Action) override;

private:
	struct FActiveItemPresentationAction
	{
		TWeakObjectPtr<AEnclaveItem> Item;
		TWeakObjectPtr<UAnimMontage> FirstPersonMontage;
		TWeakObjectPtr<UAnimMontage> ThirdPersonMontage;
		EEnclaveItemPresentationActionLifecycle Lifecycle = EEnclaveItemPresentationActionLifecycle::OneShot;
		float Duration = 0.0f;
		float StartTimeSeconds = 0.0f;
		bool bChoreographed = false;
		bool bItemPresentationStarted = false;
	};

	bool ShouldRunTransientPresentation() const;
	bool AttachItem(AEnclaveItem* Item);
	void SetItemPresentationHidden(AEnclaveItem* Item, bool bHidden) const;
	bool CommitPendingWieldedAttachment(AEnclaveItem* Item);
	void ArmPendingWieldedAttachmentFallback(float Duration);
	void ClearPendingWieldedAttachmentFallback();
	void HandlePendingWieldedAttachmentFallback();
	void LinkAnimLayers(const FGameplayTag& ItemType);
	void UnlinkPresentedAnimLayers();
	bool PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const;
	void StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const;
	/** Bridges the Focus duration event to the avatar's local view contract. */
	void HandleFocusViewPresentation(const FEnclaveItemPresentationActionEvent& Event);
	void BeginActiveItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event, const FEnclaveItemPresentationMontages* Presentation, bool bChoreographed);
	bool EndActiveItemPresentationAction(const FGameplayTag& Action);
	void EndAllActiveItemPresentationActions();

	/** How equipment is presented on this avatar body plan. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Equipment")
	TObjectPtr<UEnclaveItemPresentationDefinition> ItemPresentationDefinition = nullptr;

	/** Blend-out used when a lasting presentation action ends before its montage naturally completes. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Equipment", meta = (ClampMin = "0.0"))
	float ActionEndedBlendOutTime = 0.1f;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh = nullptr;

	/** Persistent wielded-item presentation actually applied by this component. */
	UPROPERTY(Transient)
	FEnclavePresentedWieldedItemState PresentedWieldedItemState;

	/**
	 * Presentation-only safety net for a choreographed Draw whose authored notify
	 * never executes. Gameplay readiness remains owned by the equip GameplayEffect.
	 */
	FTimerHandle PendingWieldedAttachmentFallbackTimerHandle;

	/**
	 * Item actions that still require presentation-lifetime bookkeeping. Duration
	 * actions remain here until Ended/teardown; choreographed one-shots remain
	 * only while waiting for their authored montage notify. Keyed by semantic
	 * Presentation.Item.Action.* so duplicate FP/TP notifies are idempotent.
	 */
	TMap<FGameplayTag, FActiveItemPresentationAction> ActiveItemPresentationActions;
};
