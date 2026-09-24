// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EnclaveAbilityGrant.generated.h"

class UGameplayAbility;

/**
 * One ability an item grants while it's wielded, paired with the semantic
 * input tag that activates it.
 *
 * Replaces the old "GrantedAbilities[0] is the primary action" convention
 * (Phase 2 shortcut). UEnclaveEquipmentComponent stamps InputTag onto the
 * granted FGameplayAbilitySpec's dynamic source tags at grant time, then on
 * input finds the spec whose SourceObject is the wielded item and whose
 * dynamic tags contain the pressed input tag - Lyra's pattern. Spec
 * SourceObject and dynamic tags both replicate to the owning client, so the
 * lookup works identically for LocalPredicted activation there.
 *
 * The same ability class may appear more than once with different input
 * tags - that is the intended route to primary/secondary fire modes on a
 * single UEnclaveGA_Fire class later.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveAbilityGrant
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/**
	 * Input that activates this ability while the item is wielded (e.g.
	 * Input.Item.Action.Primary, Input.Item.Action.Reload). Leave empty for
	 * a granted ability that is never input-activated (a passive, or one
	 * triggered only by gameplay events).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Input.Item"))
	FGameplayTag InputTag;
};
