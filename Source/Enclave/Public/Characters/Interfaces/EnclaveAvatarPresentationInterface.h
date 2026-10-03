// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "EnclaveAvatarPresentationInterface.generated.h"

class AActor;
class AEnclaveItem;

UENUM(BlueprintType)
enum class EEnclaveItemPresentationActionLifecycle : uint8
{
	/** One Started event; no matching Ended event is expected. */
	OneShot,

	/** Started remains active until a matching Ended event arrives or presentation is torn down. */
	Duration
};

UENUM(BlueprintType)
enum class EEnclaveItemPresentationActionPhase : uint8
{
	/** The action began. Instant actions (Fire, DryFire) only ever send this. */
	Started,

	/** A lasting action (Reload) finished - completed or cancelled alike. */
	Ended
};

/**
 * A semantic item action that has already occurred in gameplay and may need
 * to be represented by the avatar.
 *
 * The event describes what happened, not how to render it. The avatar
 * presentation implementation decides whether that means a montage, camera
 * response, audio, VFX, or no presentation at all.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemPresentationActionEvent
{
	GENERATED_BODY()

public:
	/** The concrete item that originated the presentation action, when available. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	TObjectPtr<AEnclaveItem> Item = nullptr;

	/** What kind of item performed the action, e.g. Weapon.Type.Firearm.Rifle. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	FGameplayTag ItemType;

	/** Semantic presentation action tag (Presentation.Item.Action.*). */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	FGameplayTag Action;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	EEnclaveItemPresentationActionPhase Phase = EEnclaveItemPresentationActionPhase::Started;

	/** Whether this action is one-shot or remains active until an Ended event. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	EEnclaveItemPresentationActionLifecycle Lifecycle = EEnclaveItemPresentationActionLifecycle::OneShot;

	/** Expected presentation window in seconds when gameplay knows it; 0 when unspecified. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	float Duration = 0.0f;
};

UINTERFACE(MinimalAPI)
class UEnclaveAvatarPresentationInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Presentation-facing contract for an avatar.
 *
 * Gameplay systems use this interface to report already-established state
 * changes and semantic events that need to be represented by the avatar.
 * Implementations may update meshes, attachment, animation, audio, camera,
 * or other presentation concerns, but they do not own or authorize gameplay
 * state.
 */
class ENCLAVE_API IEnclaveAvatarPresentationInterface
{
	GENERATED_BODY()

public:
	/** Present Item as the avatar's currently wielded item. */
	virtual void PresentWieldedItem(AEnclaveItem* Item) = 0;

	/** Remove the persistent wielded-item presentation currently applied by the avatar. */
	virtual void ClearWieldedItemPresentation() = 0;

	/** Present a semantic action performed by the avatar's currently wielded item. */
	virtual void HandleItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event) = 0;

	/**
	 * Reports that an avatar montage reached an authored semantic choreography
	 * point for Action. Presentation may use that point for item-local animation
	 * or a visual transition such as completing a Draw attachment. It never
	 * authorizes or completes gameplay.
	 */
	virtual void HandleItemPresentationChoreography(const FGameplayTag& Action) = 0;

	/** Find the presentation implementation associated with Actor, if one exists. */
	static IEnclaveAvatarPresentationInterface* FindAvatarPresentation(AActor* Actor);
};
