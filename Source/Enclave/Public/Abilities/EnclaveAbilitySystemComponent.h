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
	/** Activate on press. Only release-aware bindings retain GAS InputPressed state. */
	bool PressAbilityInput(FGameplayAbilitySpecHandle Handle, bool bRoutesReleaseEvents);
	/** Send release to the same ability spec, not one resolved from current equipment. */
	void ReleaseAbilityInput(FGameplayAbilitySpecHandle Handle);

protected:
	virtual void AbilitySpecInputPressed(FGameplayAbilitySpec& Spec) override;
	virtual void AbilitySpecInputReleased(FGameplayAbilitySpec& Spec) override;
};
