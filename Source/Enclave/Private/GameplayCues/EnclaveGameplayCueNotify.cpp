// Copyright Lightcarrier Studios

#include "GameplayCues/EnclaveGameplayCueNotify.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Equipment/Items/EnclaveItem.h"

bool UEnclaveGameplayCueNotify::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardCue(MyTarget, EGameplayCueEvent::Executed, Parameters);
}

bool UEnclaveGameplayCueNotify::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardCue(MyTarget, EGameplayCueEvent::OnActive, Parameters);
}

bool UEnclaveGameplayCueNotify::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardCue(MyTarget, EGameplayCueEvent::WhileActive, Parameters);
}

bool UEnclaveGameplayCueNotify::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardCue(MyTarget, EGameplayCueEvent::Removed, Parameters);
}

bool UEnclaveGameplayCueNotify::ForwardCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const
{
	if (!PresentationAction.IsValid())
	{
		return false;
	}

	EEnclaveItemPresentationActionPhase Phase = EEnclaveItemPresentationActionPhase::Started;
	float Duration = 0.0f;

	switch (PresentationMode)
	{
	case EEnclaveGameplayCuePresentationMode::OneShot:
		if (EventType != EGameplayCueEvent::Executed)
		{
			return false;
		}
		break;

	case EEnclaveGameplayCuePresentationMode::Duration:
		if (EventType == EGameplayCueEvent::WhileActive)
		{
			Phase = EEnclaveItemPresentationActionPhase::Started;
			Duration = Parameters.RawMagnitude;
		}
		else if (EventType == EGameplayCueEvent::Removed)
		{
			Phase = EEnclaveItemPresentationActionPhase::Ended;
		}
		else
		{
			return false;
		}
		break;

	default:
		return false;
	}

	return ForwardToAvatarPresentation(MyTarget, Phase, Duration, Parameters);
}

bool UEnclaveGameplayCueNotify::ForwardToAvatarPresentation(AActor* MyTarget, EEnclaveItemPresentationActionPhase Phase, float Duration, const FGameplayCueParameters& Parameters) const
{
	AEnclaveItem* Item = Cast<AEnclaveItem>(Parameters.EffectCauser.Get());

	AActor* AvatarActor = MyTarget;

	if (!IsValid(AvatarActor))
	{
		AvatarActor = Parameters.Instigator.Get();
	}

	if (!IsValid(AvatarActor) && IsValid(Item))
	{
		AvatarActor = Item->GetOwner();
	}

	IEnclaveAvatarPresentationInterface* AvatarPresentation =
		IEnclaveAvatarPresentationInterface::FindAvatarPresentation(AvatarActor);

	if (!AvatarPresentation)
	{
		return false;
	}

	FEnclaveItemPresentationActionEvent Event;
	Event.Item = Item;
	Event.ItemType = IsValid(Item) ? Item->GetItemType() : FGameplayTag();
	Event.Action = PresentationAction;
	Event.Phase = Phase;
	Event.Duration = Duration;

	AvatarPresentation->HandleItemPresentationAction(Event);
	return true;
}
