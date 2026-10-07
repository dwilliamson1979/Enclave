// Copyright Lightcarrier Studios

#include "Combat/EnclaveDamageStatics.h"

#include "GameFramework/Actor.h"

#include "Combat/Interfaces/EnclaveDamageReceiverInterface.h"

bool UEnclaveDamageStatics::ApplyDamage(AActor* Target, const FEnclaveDamageRequest& DamageRequest)
{
	if (!IsValid(Target) || !Target->HasAuthority() || DamageRequest.Damage <= 0.0f)
	{
		return false;
	}

	IEnclaveDamageReceiverInterface* DamageReceiver = Target->FindComponentByInterface<IEnclaveDamageReceiverInterface>();
	return DamageReceiver ? DamageReceiver->ReceiveDamage(DamageRequest) : false;
}
