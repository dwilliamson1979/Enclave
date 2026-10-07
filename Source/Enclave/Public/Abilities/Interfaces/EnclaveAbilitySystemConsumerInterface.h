// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveAbilitySystemConsumerInterface.generated.h"

class UAbilitySystemComponent;

UINTERFACE(MinimalAPI)
class UEnclaveAbilitySystemConsumerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for avatar-owned components that depend on the avatar's current
 * Ability System Component.
 *
 * The ASC may outlive the avatar (for example, when it is owned by
 * PlayerState), so consumers must treat initialization and uninitialization
 * as an explicit lifecycle rather than resolving the ASC ad hoc forever.
 */
class ENCLAVE_API IEnclaveAbilitySystemConsumerInterface
{
	GENERATED_BODY()

public:
	/** Called after the ASC's actor info has been initialized for this avatar. */
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent) = 0;

	/** Called when this avatar is no longer the active consumer of the ASC. */
	virtual void UninitializeFromAbilitySystem() = 0;
};
