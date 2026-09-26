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
