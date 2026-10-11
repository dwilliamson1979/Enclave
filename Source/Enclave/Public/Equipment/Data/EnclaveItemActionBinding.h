// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EnclaveItemActionBinding.generated.h"

/** Explicit per-slot routing policy. Unlisted slots keep legacy ability-grant behavior. */
UENUM(BlueprintType)
enum class EEnclaveItemActionBindingMode : uint8
{
	ItemAbility UMETA(DisplayName = "Item Ability"),
	CharacterFallback UMETA(DisplayName = "Character Fallback"),
	Suppressed UMETA(DisplayName = "Suppressed")
};

/**
 * Overrides resolution of an Input.Item.Action.* slot without changing the
 * physical input mapping or the abilities granted by the item.
 *
 * ItemAbility: resolve the matching item-granted ability spec as before.
 * CharacterFallback: reserve the slot for a character-owned ability; the
 *                    dispatch and held-input lifecycle arrive in later phases.
 * Suppressed: consume this slot intentionally without activating an ability.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemActionBinding
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input.Item.Action"))
	FGameplayTag InputTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EEnclaveItemActionBindingMode Mode = EEnclaveItemActionBindingMode::ItemAbility;

	/** Target semantic character command (e.g. Input.Avatar.Aim) for CharacterFallback.
	 * Used by the future character ability dispatcher; not activated in Phase 1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input", EditCondition = "Mode == EEnclaveItemActionBindingMode::CharacterFallback", EditConditionHides))
	FGameplayTag CharacterInputTag;
};
