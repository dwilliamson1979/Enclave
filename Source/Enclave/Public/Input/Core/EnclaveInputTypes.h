#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputActionValue.h"
#include "InputTriggers.h"
#include "EnclaveInputTypes.generated.h"

class UInputAction;

/**
 * Defines how an Enhanced Input action maps to a semantic Enclave input tag.
 *
 * Input actions represent Enhanced Input concepts.
 * Input tags represent gameplay intent within the Enclave input framework.
 *
 * Keeping this mapping here allows the same input infrastructure to support
 * different control modes without requiring custom UInputAction subclasses.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveInputActionBinding
{
	GENERATED_BODY()

	/**
	 * Enhanced Input action that generates the input.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InputAction = nullptr;

	/**
	 * Semantic gameplay tag describing the intent represented by this action.
	 *
	 * Examples:
	 * - Input.Avatar.Move
	 * - Input.Action.Item.Primary
	 * - Input.Drone.Ascend
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag InputTag;

	/**
	 * Enhanced Input trigger events that should be routed for this action.
	 *
	 * An action may need more than one event. For example, an action could
	 * respond to Started and Completed, while a continuous action could
	 * respond to Triggered.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<ETriggerEvent> TriggerEvents;
};

/**
 * Describes where a semantic input handler expects its command to execute.
 *
 * Local:
 *   The handler executes on the machine that received the physical input.
 *   This includes local movement/view handling and GAS abilities that own
 *   their own prediction/networking model.
 *
 * ServerAuthoritative:
 *   The owning client's router relays the semantic event to the server and
 *   does not execute that handler locally. The authoritative router rechecks
 *   avatar input policy before dispatching it.
 */
UENUM()
enum class EEnclaveInputNetworkPolicy : uint8
{
	Local,
	ServerAuthoritative
};

/**
 * Framework-neutral representation of a semantic player input event.
 *
 * The PlayerController converts Enhanced Input callbacks into this structure.
 * From that point onward, gameplay systems operate on semantic input rather
 * than physical keys, buttons, or concrete UInputAction assets.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveInputEvent
{
	GENERATED_BODY()

	FEnclaveInputEvent() = default;

	FEnclaveInputEvent(const FGameplayTag& InInputTag, const FInputActionValue& InValue, ETriggerEvent InTriggerEvent, bool bInRoutesReleaseEvents = false) : InputTag(InInputTag), Value(InValue), TriggerEvent(InTriggerEvent), bRoutesReleaseEvents(bInRoutesReleaseEvents)
	{
	}

	/**
	 * Semantic gameplay intent represented by this event.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	FGameplayTag InputTag;

	/**
	 * Value supplied by Enhanced Input.
	 *
	 * Depending on the input action, this may represent a bool, float,
	 * FVector2D, or FVector.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	FInputActionValue Value;

	/**
	 * Enhanced Input lifecycle event that generated this input event.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	ETriggerEvent TriggerEvent = ETriggerEvent::None;

	/** True when this input binding is configured to route Completed or Canceled. */
	UPROPERTY(BlueprintReadOnly, Category = "Input")
	bool bRoutesReleaseEvents = false;

	/**
	 * Returns true when this event contains enough information to be routed.
	 */
	bool IsValid() const
	{
		return InputTag.IsValid() && TriggerEvent != ETriggerEvent::None;
	}
};