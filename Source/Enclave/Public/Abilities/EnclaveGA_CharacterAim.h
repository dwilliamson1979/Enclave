// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveGameplayAbility.h"
#include "EnclaveGA_CharacterAim.generated.h"

/** Phase 4B hold/release character action proof. ADS/camera behavior is intentionally absent. */
UCLASS()
class ENCLAVE_API UEnclaveGA_CharacterAim : public UEnclaveGameplayAbility
{
    GENERATED_BODY()

public:
    UEnclaveGA_CharacterAim();

protected:
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

    UFUNCTION()
    void OnInputReleased(float TimeHeld);
};
