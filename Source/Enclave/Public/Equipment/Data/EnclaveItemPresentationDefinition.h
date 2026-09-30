// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimInstance.h"
#include "EnclaveItemPresentationDefinition.generated.h"

class UAnimMontage;

/**
 * Determines when the item-local portion of a semantic presentation action begins.
 *
 * Immediate starts the item as soon as the presentation action starts.
 * Choreographed waits until the avatar animation reaches an authored semantic
 * choreography point (currently emitted by UEnclaveAnimNotify_ItemPresentationAction).
 */
UENUM(BlueprintType)
enum class EEnclaveItemPresentationTiming : uint8
{
	Immediate,
	Choreographed
};

/**
 * Avatar presentation data for one item action (Presentation.Item.Action.*) with
 * one item type - e.g. Fire with a rifle: FP_AM_Rifle_Fire on the arms,
 * AM_MM_Rifle_Fire on the body. Either montage may be empty.
 *
 * Third-person montages are expected on the DefaultGroup.UpperBody slot so
 * the legs keep moving underneath; first-person ones on DefaultSlot.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemPresentationMontages
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> FirstPerson = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> ThirdPerson = nullptr;

	/** When the participating item's own presentation should begin for this action. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	EEnclaveItemPresentationTiming ItemPresentationTiming = EEnclaveItemPresentationTiming::Immediate;
};

/**
 * How one avatar presents one item type: where it attaches (sockets) and -
 * as the avatar-animation work lands - which animation layers and item-action
 * montages it uses. Everything in a row is specific to the avatar's
 * skeleton, which is why it lives here and not on the item.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemPresentationMapping
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName FirstPersonWieldedSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName ThirdPersonWieldedSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName StowedSocket = NAME_None;

	/**
	 * Layer ABP linked into this avatar's first-person (arms) anim graph
	 * while an item of this type is wielded - e.g. ABP_FP_RifleLayers. Must
	 * implement the ALI the avatar's first-person main ABP exposes and target
	 * that mesh's skeleton. Empty = keep the main ABP's default layers.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> FirstPersonAnimLayers;

	/** Third-person (body) counterpart of FirstPersonAnimLayers - e.g. ABP_TP_RifleLayers. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSubclassOf<UAnimInstance> ThirdPersonAnimLayers;

	/**
	 * This body's montages per item presentation action (Presentation.Item.Action.Draw, .Fire,
	 * .DryFire, .Reload) for this item type. An action with no entry plays
	 * nothing on the avatar - the item's own weapon-mesh montage still plays.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation", meta = (Categories = "Presentation.Item.Action", ForceInlineRow))
	TMap<FGameplayTag, FEnclaveItemPresentationMontages> ItemActionMontages;
};

/**
 * Defines how item classifications map to presentation data for an avatar.
 *
 * The item identifies what it is. The avatar selects a presentation definition
 * that determines how items of that type are physically presented.
 *
 * One asset per avatar skeleton/body plan (e.g. DA_ItemPresentation_Humanoid).
 * Renamed from UEnclaveItemAttachmentDefinition 2026-09-26 once it grew
 * beyond sockets.
 *
 * Lookup falls back up the tag hierarchy: an item typed
 * Weapon.Type.Rifle.Marksman with no row of its own uses the
 * Weapon.Type.Rifle row, so new item variants work on every avatar without
 * touching this asset until they need something different.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveItemPresentationDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Most specific row for ItemType, walking up its parent tags; nullptr if none match. */
	const FEnclaveItemPresentationMapping* FindPresentationMapping(const FGameplayTag& ItemType) const;

	/**
	 * Montages for Action on ItemType's row (same parent-tag fallback as
	 * FindPresentationMapping). Uses the most specific row only: a row that
	 * exists but has no entry for Action plays nothing rather than borrowing
	 * a parent row's.
	 */
	const FEnclaveItemPresentationMontages* FindItemPresentationMontages(const FGameplayTag& ItemType, const FGameplayTag& Action) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (AllowPrivateAccess = "true"))
	TMap<FGameplayTag, FEnclaveItemPresentationMapping> ItemPresentationMappings;
};