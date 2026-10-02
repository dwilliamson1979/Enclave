// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "EnclaveAvatarPresentationComponent.generated.h"

class AEnclaveItem;
class UEnclaveItemPresentationDefinition;
class USkeletalMeshComponent;
class UAnimMontage;

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
	virtual void ClearWieldedItemPresentation(AEnclaveItem* Item) override;
	virtual void HandleItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event) override;
	virtual void HandleItemPresentationChoreography(const FGameplayTag& Action) override;

private:
	struct FActiveChoreographedItemAction
	{
		TWeakObjectPtr<AEnclaveItem> Item;
		float Duration = 0.0f;
		float StartTimeSeconds = 0.0f;
		bool bItemPresentationStarted = false;
	};

	bool ShouldRunTransientPresentation() const;
	bool AttachItem(AEnclaveItem* Item);
	void LinkAnimLayers(const FGameplayTag& ItemType);
	void UnlinkAnimLayers(const FGameplayTag& ItemType);
	bool MontageContainsChoreographyNotify(const UAnimMontage* Montage, const FGameplayTag& Action) const;
	bool PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const;
	void StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const;
	void BeginChoreographedItemAction(const FEnclaveItemPresentationActionEvent& Event);
	void EndChoreographedItemAction(const FGameplayTag& Action);

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

	/**
	 * Lasting item actions whose item-local animation is waiting for an
	 * authored avatar AnimNotify. Keyed by semantic Presentation.Item.Action.*
	 * tag so duplicate FP/TP notifies are naturally idempotent.
	 */
	TMap<FGameplayTag, FActiveChoreographedItemAction> ActiveChoreographedItemActions;
};
