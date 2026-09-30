// Copyright Lightcarrier Studios

#include "Animation/Notifies/EnclaveAnimNotify_ItemPresentationAction.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Components/SkeletalMeshComponent.h"

void UEnclaveAnimNotify_ItemPresentationAction::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!IsValid(MeshComp) || !Action.IsValid())
	{
		return;
	}

	IEnclaveAvatarPresentationInterface* AvatarPresentation =
		IEnclaveAvatarPresentationInterface::FindAvatarPresentation(MeshComp->GetOwner());

	if (AvatarPresentation)
	{
		AvatarPresentation->HandleItemPresentationChoreography(Action);
	}
}

FString UEnclaveAnimNotify_ItemPresentationAction::GetNotifyName_Implementation() const
{
	return Action.IsValid()
		? FString::Printf(TEXT("Item Presentation: %s"), *Action.ToString())
		: TEXT("Item Presentation Action");
}
