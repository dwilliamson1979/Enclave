// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Abilities/EnclaveAbilityGrant.h"
#include "EnclaveItem.generated.h"

class USceneComponent;
class USkeletalMeshComponent;

/**
 * Base actor for a physical runtime item.
 *
 * This represents an item that exists physically in the world or is being
 * presented by an actor. It does not represent an inventory entry.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveItem : public AActor
{
	GENERATED_BODY()

public:
	AEnclaveItem();

	virtual FGameplayTag GetItemType() const;

	/**
	 * Returns whether this item declares the exact semantic capability tag.
	 * Base items declare no capabilities; concrete item types may source them
	 * from their authored definition data.
	 */
	virtual bool HasItemCapability(const FGameplayTag& CapabilityTag) const;

	/**
	 * Gameplay-authoritative time, in seconds, from selecting this item until
	 * Equipment considers it ready for item actions. Presentation may visually
	 * represent the same transition, but animation never determines readiness.
	 * Base items are instant unless a concrete item type supplies a duration.
	 */
	virtual float GetEquipDuration() const;

	/**
	 * Starts this item's local cosmetic response for a semantic presentation
	 * action. AvatarPresentation coordinates the overall avatar/item sequence
	 * and calls this only after gameplay has already established the action.
	 * Duration is the local presentation window available from this point; for
	 * notify-driven choreography it may be the remaining gameplay duration.
	 * Base items have no local response.
	 */
	virtual void StartPresentationAction(const FGameplayTag& Action, float Duration);

	/**
	 * Ends this item's local cosmetic response for a lasting semantic
	 * presentation action. Base items have no local response.
	 */
	virtual void EndPresentationAction(const FGameplayTag& Action);

	/**
	 * Abilities (each paired with the input tag that activates it)
	 * EquipmentComponent should grant to the wielder's ASC while this item
	 * is wielded, and remove the moment it stops being wielded. Base
	 * implementation grants nothing - most items (e.g. future
	 * non-actionable items) have no abilities of their own.
	 */
	virtual TArray<FEnclaveAbilityGrant> GetAbilityGrants() const { return {}; }

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Presentation")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Presentation")
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;
};
