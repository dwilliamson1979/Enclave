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

private:
	void AttachItem(AEnclaveItem* Item);
	void LinkAnimLayers(const FGameplayTag& ItemType);
	void UnlinkAnimLayers(const FGameplayTag& ItemType);
	void PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const;
	void StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const;

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
};
