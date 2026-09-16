// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
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