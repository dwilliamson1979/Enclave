#include "Input/Components/EnclaveInputRouterComponent.h"

#include "Input/Interfaces/EnclaveAvatarInputPolicyInterface.h"

UEnclaveInputRouterComponent::UEnclaveInputRouterComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;

	// Required for the component-owned ServerRouteInput RPC. The owning Pawn
	// supplies network ownership; this component supplies the centralized
	// semantic input boundary.
	SetIsReplicatedByDefault(true);
}

void UEnclaveInputRouterComponent::InitializeComponent()
{
	Super::InitializeComponent();

	AvatarInputPolicy = Cast<IEnclaveAvatarInputPolicyInterface>(GetOwner());
}

bool UEnclaveInputRouterComponent::RouteInput(const FEnclaveInputEvent& InputEvent)
{
	if (!CanRouteInput(InputEvent))
	{
		return false;
	}

	return RouteInputInternal(InputEvent, false);
}

bool UEnclaveInputRouterComponent::CanRouteInput(const FEnclaveInputEvent& InputEvent) const
{
	if (!InputEvent.IsValid())
	{
		return false;
	}

	// Releases are cleanup edges: gameplay policy may block new presses,
	// but must never suppress release/cancel for an already-held ability.
	const bool bIsRelease = InputEvent.TriggerEvent == ETriggerEvent::Completed ||
		InputEvent.TriggerEvent == ETriggerEvent::Canceled;
	return bIsRelease || AvatarInputPolicy == nullptr || AvatarInputPolicy->CanProcessAvatarInput(InputEvent);
}

bool UEnclaveInputRouterComponent::RouteInputInternal(const FEnclaveInputEvent& InputEvent, bool bServerAuthoritativeOnly)
{
	RemoveInvalidHandlers();

	const AActor* OwnerActor = GetOwner();
	const bool bHasAuthority = IsValid(OwnerActor) && OwnerActor->HasAuthority();

	for (FEnclaveInputHandlerEntry& Entry : Handlers)
	{
		if (!Entry.MatchesInput(InputEvent.InputTag))
		{
			continue;
		}

		const EEnclaveInputNetworkPolicy NetworkPolicy = Entry.HandlerInterface->GetInputNetworkPolicy(InputEvent);

		if (bServerAuthoritativeOnly)
		{
			if (NetworkPolicy != EEnclaveInputNetworkPolicy::ServerAuthoritative)
			{
				continue;
			}

			if (Entry.HandlerInterface->HandleInput(InputEvent))
			{
				return true;
			}

			continue;
		}

		if (NetworkPolicy == EEnclaveInputNetworkPolicy::ServerAuthoritative && !bHasAuthority)
		{
			ServerRouteInput(InputEvent);
			return true;
		}

		if (Entry.HandlerInterface->HandleInput(InputEvent))
		{
			return true;
		}
	}

	return false;
}

void UEnclaveInputRouterComponent::ServerRouteInput_Implementation(FEnclaveInputEvent InputEvent)
{
	// Re-evaluate the same avatar policy against authoritative state. A client
	// may legitimately be one replication interval behind; the server decides.
	if (!CanRouteInput(InputEvent))
	{
		return;
	}

	// Security boundary: the generic RPC may only invoke handlers that explicitly
	// opted this input into server-authoritative routing.
	RouteInputInternal(InputEvent, true);
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