// Copyright Lightcarrier Studios
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "EnclaveAbilitySystemComponent.generated.h"

/** GAS input edges for an already-resolved ability spec. Item and character routing live elsewhere. */
UCLASS()
class ENCLAVE_API UEnclaveAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	/** Activate a press-only ability without latching GAS InputPressed. */
	bool ActivateAbilityFromInput(FGameplayAbilitySpecHandle Handle);
	/** Press a release-aware ability and retain GAS InputPressed until ReleaseAbilityInput. */
	bool PressAbilityInput(FGameplayAbilitySpecHandle Handle);
	/** Send release to the same ability spec, not one resolved from current equipment. */
	void ReleaseAbilityInput(FGameplayAbilitySpecHandle Handle);

protected:
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
};
