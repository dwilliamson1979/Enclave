// Copyright Lightcarrier Studios

#include "GameplayCues/EnclaveGameplayCueNotify.h"

#include "GameplayCues/Interfaces/EnclaveGameplayCueResponderInterface.h"

bool UEnclaveGameplayCueNotify::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	AActor* ResponderActor = ResolveCueResponder(MyTarget, Parameters);

	if (!IsValid(ResponderActor))
	{
		return false;
	}

	IEnclaveGameplayCueResponderInterface* Responder = Cast<IEnclaveGameplayCueResponderInterface>(ResponderActor);

	if (!Responder)
	{
		return false;
	}

	return Responder->HandleGameplayCue(GetCueTag(Parameters), Parameters);
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
