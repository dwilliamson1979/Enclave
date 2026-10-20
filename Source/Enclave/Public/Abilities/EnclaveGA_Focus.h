// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveItemGameplayAbility.h"
#include "EnclaveGA_Focus.generated.h"

/**
 * Hold-to-Focus gameplay lifecycle for any currently wielded item that grants it.
 *
 * Phase 1 deliberately has no camera/ADS side effects. The item's grant supplies
 * the input binding; GAS supplies readiness, input release, cancellation,
 * prediction, and the observable State.Item.Aiming tag. Item-specific camera
 * settings, toggle mode, and zoom belong to later Focus/presentation work.
 *
 * Input configuration MUST route Started plus Completed/Canceled for this
 * ability, otherwise a held Focus activation will never see its release.
 */
UCLASS()
class ENCLAVE_API UEnclaveGA_Focus : public UEnclaveItemGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGA_Focus();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);
};
