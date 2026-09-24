// Copyright Lightcarrier Studios

#include "GameplayCues/EnclaveGameplayCueNotify.h"

#include "GameplayCues/Interfaces/EnclaveGameplayCueResponderInterface.h"

bool UEnclaveGameplayCueNotify::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardToResponder(MyTarget, EGameplayCueEvent::Executed, Parameters);
}

bool UEnclaveGameplayCueNotify::OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardToResponder(MyTarget, EGameplayCueEvent::OnActive, Parameters);
}

bool UEnclaveGameplayCueNotify::WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardToResponder(MyTarget, EGameplayCueEvent::WhileActive, Parameters);
}

bool UEnclaveGameplayCueNotify::OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	return ForwardToResponder(MyTarget, EGameplayCueEvent::Removed, Parameters);
}

bool UEnclaveGameplayCueNotify::ForwardToResponder(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const
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
		UE_LOG(LogTemp, Verbose, TEXT("%s: %s resolved no cue responder for event %d"), *GetName(), *GetNameSafe(ResponderActor), static_cast<int32>(EventType));
		return false;
	}

	return Responder->HandleGameplayCue(GetCueTag(Parameters), EventType, Parameters);
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
