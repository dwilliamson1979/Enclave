// Copyright Lightcarrier Studios

#include "Player/EnclavePlayerState.h"

#include "Abilities/EnclaveAbilitySystemComponent.h"
#include "Abilities/Attributes/EnclaveAttributeSet.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

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

void AEnclavePlayerState::SetTeamId(uint8 NewTeamId)
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("SetTeamId called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	if(TeamId == NewTeamId)
	{
		return;
	}

	TeamId = NewTeamId;
	OnRep_TeamId();
}

void AEnclavePlayerState::AddKill()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddKill called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Kills++;
}

void AEnclavePlayerState::AddDeath()
{
	if(!HasAuthority())
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("AddDeath called on non-authority PlayerState. This should only be called on the server."));
		return;
	}

	Deaths++;
}

void AEnclavePlayerState::Reset()
{
	Super::Reset(); //Handles Score reset.

	Kills = 0;
	Deaths = 0;
}

void AEnclavePlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if(AEnclavePlayerState* EnclavePlayerState = Cast<AEnclavePlayerState>(PlayerState))
	{
		EnclavePlayerState->Kills = Kills;
		EnclavePlayerState->Deaths = Deaths;
		EnclavePlayerState->TeamId = TeamId;
	}
}

void AEnclavePlayerState::OnRep_TeamId()
{
	OnTeamChanged.Broadcast(this, TeamId);
}

void AEnclavePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclavePlayerState, Kills);
	DOREPLIFETIME(AEnclavePlayerState, Deaths);
	DOREPLIFETIME(AEnclavePlayerState, TeamId);
}