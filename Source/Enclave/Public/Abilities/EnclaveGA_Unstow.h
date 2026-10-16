// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveItemGameplayAbility.h"
#include "EnclaveGA_Unstow.generated.h"

/** Item-granted action; consumes an input to restore the saved Equipment selection. */
UCLASS()
class ENCLAVE_API UEnclaveGA_Unstow : public UEnclaveItemGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Unstow();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
