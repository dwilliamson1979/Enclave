// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Abilities/EnclaveAbilityGrant.h"
#include "Equipment/Data/EnclaveItemDefinition.h"
#include "EnclaveItem.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UGameplayEffect;

/**
 * Base actor for a physical runtime item.
 *
 * This represents an item that exists physically in the world or is being
 * presented by an actor. It does not represent an inventory entry. Generic
 * authored item data is sourced from ItemDefinition; concrete subclasses add
 * only runtime behavior specific to their domain.
 */
UCLASS(Abstract, Blueprintable)
class ENCLAVE_API AEnclaveItem : public AActor
{
	GENERATED_BODY()

public:
	AEnclaveItem();

	/** Definition supplying this actor's generic authored item data. */
	virtual const UEnclaveItemDefinition* GetItemDefinition() const;

	/** Retrieve an item definition of a specific subtype; returns nullptr on a type mismatch. */
	template <typename TDefinition>
	const TDefinition* GetItemDefinitionAs() const
	{
		static_assert(TIsDerivedFrom<TDefinition, UEnclaveItemDefinition>::IsDerived,
			"TDefinition must derive from UEnclaveItemDefinition.");
		return Cast<TDefinition>(GetItemDefinition());
	}

	/** Blueprint access to the authored definition; cast the result to the desired subtype. */
	UFUNCTION(BlueprintPure, Category = "Item", meta = (DisplayName = "Get Item Definition"))
	UEnclaveItemDefinition* GetItemDefinitionForBlueprint() const;

	virtual FGameplayTag GetItemType() const;

	/** Returns whether this item declares the exact semantic capability tag. */
	virtual bool HasItemCapability(const FGameplayTag& CapabilityTag) const;

	/**
	 * Gameplay-authoritative time, in seconds, from selecting this item until
	 * Equipment considers it ready for item actions. Presentation may visually
	 * represent the same transition, but animation never determines readiness.
	 * A zero duration means there is no equip transition to present: attachment
	 * is immediate and Draw presentation is skipped.
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
	 * Abilities (each paired with the input tag that activates it) granted to
	 * the wielder's ASC while this item is wielded and removed when it stops
	 * being wielded. Empty is valid for non-actionable items.
	 */
	virtual TArray<FEnclaveAbilityGrant> GetAbilityGrants() const;

	/** Gameplay Effects provided while this item occupies any valid Equipment slot. */
	virtual TArray<TSubclassOf<UGameplayEffect>> GetEffectsWhileEquipped() const;

	/** Gameplay Effects provided only while this item is the active wielded selection. */
	virtual TArray<TSubclassOf<UGameplayEffect>> GetEffectsWhileWielded() const;

	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }
	USkeletalMeshComponent* GetThirdPersonMesh() const { return ThirdPersonMesh; }

protected:
	virtual void BeginPlay() override;

	/** Applies the generic presentation data supplied by GetItemDefinition(). */
	virtual void ApplyItemDefinition();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Presentation")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item|Presentation")
	TObjectPtr<USkeletalMeshComponent> ThirdPersonMesh;

private:
	/** Generic authored definition for this physical item. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveItemDefinition> ItemDefinition;
};
