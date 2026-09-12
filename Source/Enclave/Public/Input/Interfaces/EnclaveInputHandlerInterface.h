#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclaveInputHandlerInterface.generated.h"

UINTERFACE(MinimalAPI)
class UEnclaveInputHandlerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for objects that participate in semantic input routing.
 *
 * Input handlers declare the input domains they are interested in and
 * receive matching FEnclaveInputEvent instances from an input router.
 *
 * Handlers may consume an event to prevent lower-priority handlers from
 * processing the same input.
 */
class ENCLAVE_API IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:

	/**
	 * Returns the semantic input domains this handler is interested in.
	 *
	 * Filters support hierarchical Gameplay Tag matching.
	 *
	 * Examples:
	 * - Input.Avatar
	 * - Input.Action.Item
	 * - Input.Action.Interact
	 * - Input.Drone
	 *
	 * A handler registered for Input.Action.Item will match child tags such as:
	 * - Input.Action.Item.Primary
	 * - Input.Action.Item.Secondary
	 * - Input.Action.Item.Reload
	 *
	 * The input router may cache these filters during registration, so this
	 * function should describe relatively stable handler capabilities rather
	 * than transient gameplay state.
	 */
	virtual void GetInputFilters(FGameplayTagContainer& OutInputFilters) const = 0;

	/**
	 * Handles a semantic input event.
	 *
	 * Return true if this handler consumes the event.
	 * Return false to allow lower-priority matching handlers to process it.
	 */
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) = 0;
};