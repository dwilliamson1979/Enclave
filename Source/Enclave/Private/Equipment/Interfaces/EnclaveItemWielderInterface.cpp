// Copyright Lightcarrier Studios

#include "Equipment/Interfaces/EnclaveItemWielderInterface.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

IEnclaveItemWielderInterface* IEnclaveItemWielderInterface::FindWielder(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (UActorComponent* WielderComponent = Actor->FindComponentByInterface(UEnclaveItemWielderInterface::StaticClass()))
	{
		return Cast<IEnclaveItemWielderInterface>(WielderComponent);
	}

	return Cast<IEnclaveItemWielderInterface>(Actor);
}
