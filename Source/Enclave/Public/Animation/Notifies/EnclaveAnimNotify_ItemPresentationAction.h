// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "EnclaveAnimNotify_ItemPresentationAction.generated.h"

/**
 * Presentation-only choreography marker placed on an avatar montage.
 *
 * When reached, it tells the avatar presentation implementation that the
 * item-local portion of the specified semantic action may begin. It does not
 * authorize gameplay, mutate ammo/state, or complete an ability.
 */
UCLASS(meta = (DisplayName = "Enclave Item Presentation Action"))
class ENCLAVE_API UEnclaveAnimNotify_ItemPresentationAction : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

	/** Semantic action this notify releases for item-local presentation. */
	const FGameplayTag& GetAction() const { return Action; }

private:
	/** Semantic Presentation.Item.Action.* action whose item-local presentation should begin here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (AllowPrivateAccess = "true", Categories = "Presentation.Item.Action"))
	FGameplayTag Action;
};
