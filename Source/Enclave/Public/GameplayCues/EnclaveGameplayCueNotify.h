// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "EnclaveGameplayCueNotify.generated.h"

/**
 * Shared base for Gameplay Cue notifies.
 *
 * Recognized item-action cues are translated into one semantic
 * FEnclaveItemPresentationActionEvent and sent to avatar presentation.
 * AvatarPresentation is the single presentation coordinator: it presents the
 * avatar and routes item-local presentation to the originating AEnclaveItem.
 *
 * Item-local presentation still starts immediately in Phase 2. A later
 * choreography phase can move that item start behind an authored AnimNotify
 * without changing this gameplay/ability boundary again.
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
	/** Translates one cue event into semantic avatar/item presentation. */
	bool ForwardCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	/**
	 * Converts recognized item-action cues into semantic avatar presentation
	 * events and sends them directly to the avatar presentation implementation.
	 */
	bool ForwardToAvatarPresentation(AActor* MyTarget, FGameplayTag CueTag, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	FGameplayTag GetCueTag(const FGameplayCueParameters& Parameters) const;
};
