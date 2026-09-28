// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Equipment/Interfaces/EnclaveItemWielderInterface.h"
#include "EnclaveWielderComponent.generated.h"

class USkeletalMeshComponent;
class UAnimMontage;
class UEnclaveItemPresentationDefinition;
class AEnclaveItem;

/**
 * The body side of wielding: takes an item in hand on the owner's meshes and
 * puts it away again (IEnclaveItemWielderInterface).
 *
 * Everything here is specific to a body's skeleton - sockets, animation
 * layers (and next, wielder montages) - and all of it is driven by
 * ItemPresentationDefinition, one asset per body plan. A different body
 * (an alien, say) reuses this component with its own meshes and its own
 * presentation asset rather than growing its character class.
 *
 * Holds no gameplay state and does not replicate: UEnclaveEquipmentComponent
 * decides what is wielded and calls this on every machine.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveWielderComponent : public UActorComponent, public IEnclaveItemWielderInterface
{
	GENERATED_BODY()

public:
	UEnclaveWielderComponent();

	/**
	 * The owner's meshes items attach to and animation layers link into.
	 * Call from the owning actor's constructor (both are its own default
	 * subobjects, so the references are fixed up per instance).
	 */
	void SetWielderMeshes(USkeletalMeshComponent* InFirstPersonMesh, USkeletalMeshComponent* InThirdPersonMesh);

	// IEnclaveItemWielderInterface
	virtual bool WieldItem(AEnclaveItem* Item) override;
	virtual void UnwieldItem(AEnclaveItem* Item) override;

	/** Plays this body's montages for the action (Started, duration-scaled) or stops them (Ended). */
	virtual void HandleWieldedItemAction(const FEnclaveWieldedItemActionEvent& Event) override;

private:
	/** Attaches Item to the owner, and its FP/TP meshes to the sockets its type's presentation row names. */
	bool AttachItem(AEnclaveItem* Item);

	/** Links/unlinks the animation layers ItemType's presentation row names, on the FP and TP meshes. */
	void LinkAnimLayers(const FGameplayTag& ItemType);
	void UnlinkAnimLayers(const FGameplayTag& ItemType);

	/** Plays Montage on MeshComp, scaled to finish in Duration seconds when Duration > 0. */
	void PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const;

	/** Stops Montage on MeshComp if it's still playing there. */
	void StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const;

	/** How items look on this body - sockets and animation layers per item type (e.g. DA_ItemPresentation_Humanoid). */
	UPROPERTY(EditDefaultsOnly, Category = "Wielder")
	TObjectPtr<UEnclaveItemPresentationDefinition> ItemPresentationDefinition = nullptr;

	/** Blend-out when a lasting action ends early (e.g. a reload cancelled by switching weapons). */
	UPROPERTY(EditDefaultsOnly, Category = "Wielder", meta = (ClampMin = "0.0"))
	float ActionEndedBlendOutTime = 0.1f;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh = nullptr;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh = nullptr;
};
