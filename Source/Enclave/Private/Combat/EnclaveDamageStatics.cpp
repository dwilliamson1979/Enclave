// Copyright Lightcarrier Studios

#include "Combat/EnclaveDamageStatics.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"

#include "Combat/Interfaces/EnclaveDamagePolicyInterface.h"
#include "Combat/Interfaces/EnclaveDamageReceiverInterface.h"

bool UEnclaveDamageStatics::ApplyDamage(AActor* Target, const FEnclaveDamageRequest& DamageRequest)
{
	if (!IsValid(Target) || !Target->HasAuthority() || DamageRequest.Damage <= 0.0f)
	{
		return false;
	}

	IEnclaveDamageReceiverInterface* DamageReceiver = Target->FindComponentByInterface<IEnclaveDamageReceiverInterface>();
	if (!DamageReceiver)
	{
		return false;
	}

	// The game mode's rules may ignore the hit, simulate it, or change it (e.g.
	// scale friendly fire). A game mode without rules applies it unchanged.
	FEnclaveDamageRequest ResolvedRequest = DamageRequest;
	const IEnclaveDamagePolicyInterface* DamagePolicy = Cast<IEnclaveDamagePolicyInterface>(Target->GetWorld()->GetAuthGameMode());
	const EEnclaveHitResponse Response = DamagePolicy
		? DamagePolicy->ResolveDamage(Target, ResolvedRequest)
		: EEnclaveHitResponse::Apply;

	switch (Response)
	{
	case EEnclaveHitResponse::Simulate:
		// A hit the attacker sees (hit marker), but health doesn't change.
		return true;
	case EEnclaveHitResponse::Apply:
		return ResolvedRequest.Damage > 0.0f && DamageReceiver->ReceiveDamage(ResolvedRequest);
	case EEnclaveHitResponse::Ignore:
	default:
		return false;
	}
}
