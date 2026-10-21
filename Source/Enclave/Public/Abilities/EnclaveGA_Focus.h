// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/EnclaveItemGameplayAbility.h"
#include "EnclaveGA_Focus.generated.h"

/**
 * Hold-to-Focus gameplay lifecycle for any currently wielded item that grants it.
 *
 * The item grants the input and resolves a Focus profile through a capability
 * interface. GAS owns readiness, release, cancellation, prediction and the
 * State.Item.Aiming tag. A duration cue coordinates presentation; the ability
 * never changes camera settings directly. Toggle mode and variable zoom are
 * separate future work.
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
	/** Add a prediction-aware duration cue, removed by GAS on EndAbility. */
	void AddFocusCue(const FGameplayAbilityActorInfo* ActorInfo);

	UFUNCTION()
	void OnInputReleased(float TimeHeld);
};
