// Copyright Lightcarrier Studios

#include "Player/EnclavePlayerState.h"

#include "Abilities/EnclaveAbilitySystemComponent.h"
#include "Abilities/Attributes/EnclaveAttributeSet.h"

AEnclavePlayerState::AEnclavePlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UEnclaveAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed: full ability/effect/cue replication to the owning client, minimal
	// to everyone else. The standard choice for a player-controlled ASC in a
	// shooter - simulated proxies don't need the same fidelity the owning
	// client does.
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UEnclaveAttributeSet>(TEXT("AttributeSet"));

	// PlayerState defaults to a low net update frequency, which is too slow
	// for ability/attribute replication to feel responsive. This is the
	// standard corrective value used across GAS projects.
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AEnclavePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

EEnclaveAttitude AEnclavePlayerState::GetAttitudeTowards(const APlayerState* Other) const
{
	// A spectator takes no part in the fight, either way round, so the answer
	// stays symmetric. IsOnlyASpectator, not IsSpectator: both are set by
	// StartSpectatingOnly, but leaving the spectating state clears only
	// IsSpectator (PlayerController.cpp:5788-5792, 5980).
	if (!Other || Other == this || IsOnlyASpectator() || Other->IsOnlyASpectator())
	{
		return EEnclaveAttitude::Neutral;
	}

	return EEnclaveAttitude::Hostile;
}
