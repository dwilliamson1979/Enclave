// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "EnclaveGameplayCueNotify.generated.h"

enum class EEnclaveItemPresentationActionPhase : uint8;
enum class EEnclaveItemPresentationActionLifecycle : uint8;

/**
 * Defines how a Gameplay Cue's lifecycle is translated into semantic
 * presentation-action phases.
 */
UENUM(BlueprintType)
enum class EEnclaveGameplayCuePresentationMode : uint8
{
	/** Executed maps to one Started presentation event. */
	OneShot,

	/** WhileActive maps to Started and Removed maps to Ended. */
	Duration
};

/**
 * Shared base for Gameplay Cue notifies that forward semantic item actions to
 * avatar presentation.
 *
 * Each concrete Gameplay Cue Blueprint owns its mapping in Class Defaults:
 * - GameplayCueTag identifies the gameplay event handled by that Blueprint.
 * - PresentationAction identifies the semantic Presentation.Item.Action.* event.
 * - PresentationMode defines how the cue lifecycle maps to Started/Ended.
 *
 * AvatarPresentation remains the single presentation coordinator: it presents
 * the avatar and routes item-local presentation to the originating AEnclaveItem.
 * It may start item-local presentation immediately or defer it to an authored
 * avatar AnimNotify. That choreography decision stays below this cue boundary.
 *
 * The Gameplay Cue Manager dispatches by exact tag match, with no built-in
 * wildcard/parent-tag routing, so a distinct notify is still required per tag.
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
	/** Semantic presentation action produced by this cue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (Categories = "Presentation.Item.Action"))
	FGameplayTag PresentationAction;

	/** How this cue's GAS lifecycle maps to presentation Started/Ended phases. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation")
	EEnclaveGameplayCuePresentationMode PresentationMode = EEnclaveGameplayCuePresentationMode::OneShot;

	/** Translates this cue's configured lifecycle into semantic avatar/item presentation. */
	bool ForwardCue(AActor* MyTarget, EGameplayCueEvent::Type EventType, const FGameplayCueParameters& Parameters) const;

	/** Sends an already-resolved semantic presentation phase to avatar presentation. */
	bool ForwardToAvatarPresentation(AActor* MyTarget, EEnclaveItemPresentationActionPhase Phase, EEnclaveItemPresentationActionLifecycle Lifecycle, float Duration, const FGameplayCueParameters& Parameters) const;
};
