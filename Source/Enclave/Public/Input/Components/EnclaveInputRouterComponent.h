#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "EnclaveInputRouterComponent.generated.h"

/**
 * Runtime registration information for one input handler.
 *
 * The router caches the handler's filters at registration time so that
 * routing does not need to repeatedly query the handler for its capabilities.
 */
USTRUCT()
struct FEnclaveInputHandlerEntry
{
	GENERATED_BODY()

	FEnclaveInputHandlerEntry() = default;

	FEnclaveInputHandlerEntry(
		UObject* InHandlerObject,
		IEnclaveInputHandlerInterface* InHandlerInterface,
		int32 InPriority,
		const FGameplayTagContainer& InInputFilters)
		: Priority(InPriority)
		, HandlerObject(InHandlerObject)
		, HandlerInterface(InHandlerInterface)
		, InputFilters(InInputFilters)
	{
	}

	/**
	 * Higher-priority handlers receive matching input first.
	 */
	UPROPERTY()
	int32 Priority = 0;

	/**
	 * Weak UObject reference used to verify that the handler still exists.
	 */
	UPROPERTY()
	TWeakObjectPtr<UObject> HandlerObject = nullptr;

	/**
	 * Cached native interface pointer used for dispatch.
	 *
	 * This pointer is valid only while HandlerObject remains valid.
	 */
	IEnclaveInputHandlerInterface* HandlerInterface = nullptr;

	/**
	 * Cached semantic input filters declared by the handler.
	 *
	 * An empty container means the handler accepts all input.
	 */
	UPROPERTY()
	FGameplayTagContainer InputFilters;

	/**
	 * Returns true while both the UObject and interface remain usable.
	 */
	bool IsValid() const
	{
		return HandlerObject.IsValid() &&
			HandlerInterface != nullptr;
	}

	/**
	 * Returns true when this handler is interested in the supplied input tag.
	 *
	 * Gameplay Tag hierarchy is respected. A filter of Input.Action.Item,
	 * for example, matches Input.Action.Item.Primary.
	 */
	bool MatchesInput(const FGameplayTag& InputTag) const
	{
		if (!InputTag.IsValid())
		{
			return false;
		}

		if (InputFilters.IsEmpty())
		{
			return true;
		}

		for (const FGameplayTag& FilterTag : InputFilters)
		{
			if (FilterTag.IsValid() &&
				InputTag.MatchesTag(FilterTag))
			{
				return true;
			}
		}

		return false;
	}
};

/**
 * Routes semantic input events to registered input handlers.
 *
 * Handlers are evaluated from highest to lowest priority. Routing stops when
 * a matching handler consumes the event.
 *
 * This component has no knowledge of concrete gameplay systems such as
 * movement, weapons, abilities, drones, or vehicles.
 */
UCLASS(ClassGroup = (Input), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveInputRouterComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UEnclaveInputRouterComponent();

	/**
	 * Routes an input event through the registered handlers.
	 *
	 * Returns true if a handler consumed the event.
	 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool RouteInput(const FEnclaveInputEvent& InputEvent);

	/**
	 * Registers a handler using the filters reported by
	 * IEnclaveInputHandlerInterface::GetInputFilters().
	 */
	void RegisterInputHandler(UObject* Handler, int32 Priority = 0);

	/**
	 * Registers a handler with one explicit input filter.
	 *
	 * This overrides the handler's declared filters for this registration.
	 */
	void RegisterInputHandler(UObject* Handler, int32 Priority, const FGameplayTag& InputFilter);

	/**
	 * Registers a handler with explicit input filters.
	 *
	 * These filters override the handler's declared filters for this
	 * registration.
	 */
	void RegisterInputHandler(UObject* Handler, int32 Priority, const FGameplayTagContainer& InputFilters);

	/**
	 * Removes a previously registered handler.
	 */
	void UnregisterInputHandler(UObject* Handler);

	/**
	 * Refreshes the cached filters for a registered handler by calling
	 * GetInputFilters() again.
	 */
	void RefreshInputHandlerFilters(UObject* Handler);

private:

	/**
	 * Sorts handlers from highest to lowest priority.
	 */
	void SortHandlers();

	/**
	 * Removes entries whose underlying UObject is no longer valid.
	 */
	void RemoveInvalidHandlers();

	UPROPERTY()
	TArray<FEnclaveInputHandlerEntry> Handlers;
};