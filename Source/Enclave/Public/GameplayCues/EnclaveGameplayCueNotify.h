// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "EnclaveGameplayCueNotify.generated.h"

/**
 * Shared base for Gameplay Cue notifies that own no presentation logic of
 * their own - they just resolve the actor a cue is actually about and
 * forward the tag to it via IEnclaveGameplayCueResponderInterface, letting that
 * actor decide how (or whether) to respond.
 *
 * The Gameplay Cue Manager dispatches by exact tag match, with no built-in
 * wildcard/parent-tag routing, so a distinct notify is still required per
 * tag - but since this class already has no tag-specific logic of its own,
 * that distinct notify doesn't need to be a native subclass either. Create
 * a Blueprint directly off this class per tag (e.g. GCN_WeaponFire,
 * GCN_WeaponReload) and set GameplayCueTag in its Class Defaults; marked
 * Abstract purely to keep this class itself out of the "pick a class to
 * create a Gameplay Cue Notify from" list, not because anything here is
 * unimplemented.
 *
 * Static, not Actor: the notify itself stays stateless even for duration
 * cues (GameplayCue.Weapon.Reload) - it forwards OnActive/WhileActive/
 * Removed along with Executed, and any state a duration cue needs (a
 * playing montage) lives on the responder, which already owns it. That
 * keeps one notify shape for every cue, and nothing to spawn or recycle per
 * execution for an ability re-executing a cue every tick (automatic fire).
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
	/**
	 * Resolves the actor this cue is actually about from
	 * Parameters.EffectCauser - the triggering ability is responsible for
	 * populating that field with whatever should receive
	 * HandleGameplayCue (see UEnclaveGA_Fire::ExecuteSourceCue). Falls back
	 * to MyTarget (GAS's own resolution of the ASC's avatar actor) only if
	 * EffectCauser isn't set, which is correct only when the avatar itself
	 * is the intended responder.
	 *
	 * Deliberately does not fall back to Parameters.SourceObject:
	 * IEnclaveGameplayCueResponderInterface::HandleGameplayCue is non-const (a
	 * responder may need to spawn components, play audio, etc.), and
	 * SourceObject is a TObjectPtr<const UObject> - EffectCauser is the
	 * field meant for exactly this kind of non-const, actor-shaped
	 * response target.
	 */
	static AActor* ResolveCueResponder(AActor* MyTarget, const FGameplayCueParameters& Parameters);

	/** Resolve + forward, shared by every event override above. */
	bool ForwardToResponder(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	FGameplayTag GetCueTag(const FGameplayCueParameters& Parameters) const;
};
