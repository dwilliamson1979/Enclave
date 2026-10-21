// Copyright Lightcarrier Studios

#include "Characters/Interfaces/EnclaveFocusViewInterface.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

IEnclaveFocusViewInterface* IEnclaveFocusViewInterface::FindFocusView(AActor* Avatar)
{
	if (!IsValid(Avatar))
	{
		return nullptr;
	}

	if (UActorComponent* ViewComponent = Avatar->FindComponentByInterface(UEnclaveFocusViewInterface::StaticClass()))
	{
		return Cast<IEnclaveFocusViewInterface>(ViewComponent);
	}

	return Cast<IEnclaveFocusViewInterface>(Avatar);
}
