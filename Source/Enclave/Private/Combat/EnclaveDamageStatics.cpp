// Copyright Lightcarrier Studios

#include "Combat/EnclaveDamageStatics.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

#include "Combat/Interfaces/EnclaveDamageReceiverInterface.h"
#include "Debug/EnclaveDebug.h"

bool UEnclaveDamageStatics::ApplyDamage(AActor* Target, const FEnclaveDamageRequest& DamageRequest)
{
	if (!IsValid(Target) || !Target->HasAuthority() || DamageRequest.Damage <= 0.0f)
	{
		return false;
	}

	IEnclaveDamageReceiverInterface* DamageReceiver = nullptr;
	TInlineComponentArray<UActorComponent*> Components;
	Target->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!IsValid(Component) || !Component->GetClass()->ImplementsInterface(UEnclaveDamageReceiverInterface::StaticClass()))
		{
			continue;
		}

		IEnclaveDamageReceiverInterface* Candidate = Cast<IEnclaveDamageReceiverInterface>(Component);
		if (!Candidate)
		{
			continue;
		}

		if (DamageReceiver)
		{
			UE_LOG(LogEnclaveCombat, Warning, TEXT("%s has multiple Enclave damage receivers. Exactly one top-level receiver is supported; damage was not applied."), *GetNameSafe(Target));
			return false;
		}

		DamageReceiver = Candidate;
	}

	return DamageReceiver ? DamageReceiver->ReceiveDamage(DamageRequest) : false;
}
