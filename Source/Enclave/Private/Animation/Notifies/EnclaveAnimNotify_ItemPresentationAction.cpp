// Copyright Lightcarrier Studios

#include "Animation/Notifies/EnclaveAnimNotify_ItemPresentationAction.h"

#include "Animation/AnimMontage.h"
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

bool UEnclaveAnimNotify_ItemPresentationAction::MontageContainsAction(const UAnimMontage* Montage, const FGameplayTag& Action)
{
	if (!IsValid(Montage) || !Action.IsValid())
	{
		return false;
	}

	// Choreography markers are intentionally montage-owned. A notify authored on
	// a source sequence may still fire while that sequence is used by a montage,
	// but it is not considered a valid Enclave choreography contract.
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UEnclaveAnimNotify_ItemPresentationAction* PresentationNotify =
			Cast<UEnclaveAnimNotify_ItemPresentationAction>(NotifyEvent.Notify.Get());

		if (PresentationNotify && PresentationNotify->GetAction().MatchesTagExact(Action))
		{
			return true;
		}
	}

	return false;
}
