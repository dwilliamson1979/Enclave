// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "EnclaveAvatarPresentationInterface.generated.h"

class AActor;
class AEnclaveItem;

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
	/** What kind of item performed the action, e.g. Weapon.Type.Firearm.Rifle. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	FGameplayTag ItemType;

	/** Semantic action tag. Currently uses the existing Wielder.Action.* tags. */
	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	FGameplayTag Action;

	UPROPERTY(BlueprintReadOnly, Category = "Presentation|Equipment")
	EEnclaveItemPresentationActionPhase Phase = EEnclaveItemPresentationActionPhase::Started;

	/** Expected length in seconds when gameplay knows it; 0 for instant actions. */
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

	/** Remove presentation associated with Item no longer being wielded. */
	virtual void ClearWieldedItemPresentation(AEnclaveItem* Item) = 0;

	/** Present a semantic action performed by the avatar's currently wielded item. */
	virtual void HandleItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event) = 0;

	/** Find the presentation implementation associated with Actor, if one exists. */
	static IEnclaveAvatarPresentationInterface* FindAvatarPresentation(AActor* Actor);
};
