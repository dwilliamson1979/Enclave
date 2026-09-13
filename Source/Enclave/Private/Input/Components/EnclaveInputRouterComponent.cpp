#include "Input/Components/EnclaveInputRouterComponent.h"

UEnclaveInputRouterComponent::UEnclaveInputRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UEnclaveInputRouterComponent::RouteInput(const FEnclaveInputEvent& InputEvent)
{
	if (!InputEvent.IsValid())
	{
		return false;
	}

	RemoveInvalidHandlers();

	for (FEnclaveInputHandlerEntry& Entry : Handlers)
	{
		if (!Entry.MatchesInput(InputEvent.InputTag))
		{
			continue;
		}

		if (Entry.HandlerInterface->HandleInput(InputEvent))
		{
			return true;
		}
	}

	return false;
}

void UEnclaveInputRouterComponent::RegisterInputHandler(UObject* Handler, int32 Priority)
{
	if (!Handler)
	{
		return;
	}

	IEnclaveInputHandlerInterface* HandlerInterface =
		Cast<IEnclaveInputHandlerInterface>(Handler);

	if (!HandlerInterface)
	{
		return;
	}

	RegisterInputHandler(
		Handler,
		Priority,
		HandlerInterface->GetInputFilters());
}

void UEnclaveInputRouterComponent::RegisterInputHandler(UObject* Handler, int32 Priority, const FGameplayTag& InputFilter)
{
	FGameplayTagContainer InputFilters;

	if (InputFilter.IsValid())
	{
		InputFilters.AddTag(InputFilter);
	}

	RegisterInputHandler(
		Handler,
		Priority,
		InputFilters);
}

void UEnclaveInputRouterComponent::RegisterInputHandler(UObject* Handler, int32 Priority, const FGameplayTagContainer& InputFilters)
{
	if (!Handler)
	{
		return;
	}

	IEnclaveInputHandlerInterface* HandlerInterface =
		Cast<IEnclaveInputHandlerInterface>(Handler);

	if (!HandlerInterface)
	{
		return;
	}

	RemoveInvalidHandlers();

	UnregisterInputHandler(Handler);

	Handlers.Emplace(
		Handler,
		HandlerInterface,
		Priority,
		InputFilters);

	SortHandlers();
}

void UEnclaveInputRouterComponent::UnregisterInputHandler(UObject* Handler)
{
	if (!Handler)
	{
		return;
	}

	Handlers.RemoveAll(
		[Handler](const FEnclaveInputHandlerEntry& Entry)
		{
			return Entry.HandlerObject.Get() == Handler;
		});
}

void UEnclaveInputRouterComponent::RefreshInputHandlerFilters(UObject* Handler)
{
	if (!Handler)
	{
		return;
	}

	IEnclaveInputHandlerInterface* HandlerInterface =
		Cast<IEnclaveInputHandlerInterface>(Handler);

	if (!HandlerInterface)
	{
		return;
	}

	for (FEnclaveInputHandlerEntry& Entry : Handlers)
	{
		if (Entry.HandlerObject.Get() == Handler)
		{
			Entry.InputFilters = HandlerInterface->GetInputFilters();
			return;
		}
	}
}

void UEnclaveInputRouterComponent::SortHandlers()
{
	Handlers.Sort(
		[](const FEnclaveInputHandlerEntry& Left,
		   const FEnclaveInputHandlerEntry& Right)
		{
			return Left.Priority > Right.Priority;
		});
}

void UEnclaveInputRouterComponent::RemoveInvalidHandlers()
{
	Handlers.RemoveAll(
		[](const FEnclaveInputHandlerEntry& Entry)
		{
			return !Entry.IsValid();
		});
}