// Copyright Lightcarrier Studios

#include "GameplayCues/EnclaveGameplayCueNotify.h"

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayCues/Interfaces/EnclaveGameplayCueResponderInterface.h"
#include "GameplayTags/EnclaveGameplayCueTags.h"
#include "GameplayTags/EnclavePresentationTags.h"
#include "Debug/EnclaveDebug.h"

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
	const FGameplayTag CueTag = GetCueTag(Parameters);
	const bool bAvatarPresentationHandled = ForwardToAvatarPresentation(MyTarget, CueTag, EventType, Parameters);
	const bool bResponderHandled = ForwardToResponder(MyTarget, CueTag, EventType, Parameters);

	return bAvatarPresentationHandled || bResponderHandled;
}

bool UEnclaveGameplayCueNotify::ForwardToAvatarPresentation(AActor* MyTarget, FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const
{
	FGameplayTag PresentationAction;
	EEnclaveItemPresentationActionPhase Phase = EEnclaveItemPresentationActionPhase::Started;
	float Duration = 0.0f;

	if (CueTag.MatchesTagExact(EnclaveGameplayTags::GameplayCue_Weapon_Fire))
	{
		if (EventType != EGameplayCueEvent::Executed)
		{
			return false;
		}

		PresentationAction = EnclaveGameplayTags::Presentation_Item_Action_Fire;
	}
	else if (CueTag.MatchesTagExact(EnclaveGameplayTags::GameplayCue_Weapon_DryFire))
	{
		if (EventType != EGameplayCueEvent::Executed)
		{
			return false;
		}

		PresentationAction = EnclaveGameplayTags::Presentation_Item_Action_DryFire;
	}
	else if (CueTag.MatchesTagExact(EnclaveGameplayTags::GameplayCue_Weapon_Reload))
	{
		if (EventType == EGameplayCueEvent::WhileActive)
		{
			PresentationAction = EnclaveGameplayTags::Presentation_Item_Action_Reload;
			Phase = EEnclaveItemPresentationActionPhase::Started;
			Duration = Parameters.RawMagnitude;
		}
		else if (EventType == EGameplayCueEvent::Removed)
		{
			PresentationAction = EnclaveGameplayTags::Presentation_Item_Action_Reload;
			Phase = EEnclaveItemPresentationActionPhase::Ended;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

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

bool UEnclaveGameplayCueNotify::ForwardToResponder(AActor* MyTarget, FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const
{
	AActor* ResponderActor = ResolveCueResponder(MyTarget, Parameters);

	if (!IsValid(ResponderActor))
	{
		return false;
	}

	IEnclaveGameplayCueResponderInterface* Responder = Cast<IEnclaveGameplayCueResponderInterface>(ResponderActor);

	if (!Responder)
	{
		// Expected for a Removed event whose parameters arrived without an
		// EffectCauser (resolution fell back to MyTarget, the avatar, which
		// isn't a responder). Verbose rather than Warning - see the PIE
		// verification note on weapon-swap-mid-reload.
		ENCLAVE_LOG(LogEnclaveGameplayCue, Verbose, "%s: %s resolved no cue responder for event %d", *GetName(), *GetNameSafe(ResponderActor), static_cast<int32>(EventType));
		return false;
	}

	return Responder->HandleGameplayCue(CueTag, EventType, Parameters);
}

FGameplayTag UEnclaveGameplayCueNotify::GetCueTag(const FGameplayCueParameters& Parameters) const
{
	// OriginalTag over GameplayCueTag: the tag actually executed for this
	// instance. Practically always the same value given exact-match
	// dispatch, but OriginalTag is the more correct one to forward.
	return Parameters.OriginalTag.IsValid() ? Parameters.OriginalTag : GameplayCueTag;
}

AActor* UEnclaveGameplayCueNotify::ResolveCueResponder(AActor* MyTarget, const FGameplayCueParameters& Parameters)
{
	AActor* EffectCauser = Parameters.EffectCauser.Get();

	if (IsValid(EffectCauser))
	{
		return EffectCauser;
	}

	return MyTarget;
}
