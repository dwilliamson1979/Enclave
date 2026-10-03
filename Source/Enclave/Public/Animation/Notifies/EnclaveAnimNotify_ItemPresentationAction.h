// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "EnclaveAnimNotify_ItemPresentationAction.generated.h"

class UAnimMontage;

/**
 * Presentation-only choreography marker placed on an avatar montage.
 *
 * When reached, it tells the avatar presentation implementation that the
 * specified semantic action reached an authored presentation choreography
 * point. It does not authorize gameplay, mutate ammo/state, or complete an ability.
 */
UCLASS(meta = (DisplayName = "Enclave Item Presentation Action"))
class ENCLAVE_API UEnclaveAnimNotify_ItemPresentationAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/**
	 * Returns whether Montage contains a matching choreography notify authored
	 * directly on the montage timeline. Source-sequence notifies are deliberately
	 * not accepted as Enclave choreography markers.
	 */
	static bool MontageContainsAction(const UAnimMontage* Montage, const FGameplayTag& Action);

	/** Semantic action whose authored presentation choreography point was reached. */
	const FGameplayTag& GetAction() const { return Action; }

private:
	/** Semantic Presentation.Item.Action.* action whose choreography point is authored here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (AllowPrivateAccess = "true", Categories = "Presentation.Item.Action"))
	FGameplayTag Action;
};
