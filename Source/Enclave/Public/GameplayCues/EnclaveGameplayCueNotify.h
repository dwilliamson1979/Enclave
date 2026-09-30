// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "EnclaveGameplayCueNotify.generated.h"

/**
 * Shared base for Gameplay Cue notifies.
 *
 * One cue event now fans out to the two presentation concerns that need it:
 * - Avatar presentation receives a semantic FEnclaveItemPresentationActionEvent
 *   directly (Fire, DryFire, Reload Started/Ended).
 * - The cue's EffectCauser still receives the original cue through
 *   IEnclaveGameplayCueResponderInterface for item-local presentation such as
 *   weapon-mesh montages, muzzle flash, and weapon audio.
 *
 * This removes the old Item -> AvatarPresentation forwarding dependency while
 * deliberately preserving current item-local presentation timing. A later
 * phase can move item choreography under AvatarPresentation without changing
 * the gameplay/ability side again.
 *
 * The Gameplay Cue Manager dispatches by exact tag match, with no built-in
 * wildcard/parent-tag routing, so a distinct notify is still required per tag.
 * Create a Blueprint directly off this class per tag (e.g. GCN_WeaponFire,
 * GCN_WeaponReload) and set GameplayCueTag in its Class Defaults.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveGameplayCueNotify : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	/** Routes one cue event to avatar presentation and the cue responder. */
	bool ForwardCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	/**
	 * Converts recognized item-action cues into semantic avatar presentation
	 * events and sends them directly to the avatar presentation implementation.
	 */
	bool ForwardToAvatarPresentation(AActor* MyTarget, FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	/**
	 * Resolves the actor this cue is actually about from Parameters.EffectCauser
	 * and forwards the original cue to IEnclaveGameplayCueResponderInterface.
	 * Falls back to MyTarget only if EffectCauser is not set.
	 *
	 * Deliberately does not fall back to Parameters.SourceObject: responder
	 * handling is non-const, while SourceObject is const.
	 */
	bool ForwardToResponder(AActor* MyTarget, FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	static AActor* ResolveCueResponder(AActor* MyTarget, const FGameplayCueParameters& Parameters);
	FGameplayTag GetCueTag(const FGameplayCueParameters& Parameters) const;
};
