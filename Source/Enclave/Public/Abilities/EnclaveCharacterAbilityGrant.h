// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "EnclaveCharacterAbilityGrant.generated.h"

class UGameplayAbility;

/** Avatar-lifetime ability grant, independent of the currently selected equipment item.
 * SourceObject is the avatar, and an optional semantic Input.Avatar tag is carried
 * on the replicated spec for a future direct avatar-action handler. */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveCharacterAbilityGrant
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
    TSubclassOf<UGameplayAbility> AbilityClass;

    /** Empty for non-input abilities. Reserved for direct avatar-action routing; not resolved by item inputs. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability", meta = (Categories = "Input.Avatar"))
    FGameplayTag InputTag;
};
