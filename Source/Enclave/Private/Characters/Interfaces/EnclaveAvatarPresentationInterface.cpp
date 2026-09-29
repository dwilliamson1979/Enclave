// Copyright Lightcarrier Studios

#include "Characters/Interfaces/EnclaveAvatarPresentationInterface.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

IEnclaveAvatarPresentationInterface* IEnclaveAvatarPresentationInterface::FindAvatarPresentation(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (UActorComponent* PresentationComponent = Actor->FindComponentByInterface(UEnclaveAvatarPresentationInterface::StaticClass()))
	{
		return Cast<IEnclaveAvatarPresentationInterface>(PresentationComponent);
	}

	return Cast<IEnclaveAvatarPresentationInterface>(Actor);
}
