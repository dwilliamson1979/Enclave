// Copyright Lightcarrier Studios

#include "Player/EnclaveTeamPlayerState.h"

#include "Net/UnrealNetwork.h"

#include "Debug/EnclaveDebug.h"

void AEnclaveTeamPlayerState::SetTeamId(uint8 NewTeamId)
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

EEnclaveAttitude AEnclaveTeamPlayerState::GetAttitudeTowards(const APlayerState* Other) const
{
	// Neutral for yourself and spectators; split the rest by team.
	const EEnclaveAttitude Attitude = Super::GetAttitudeTowards(Other);
	if (Attitude == EEnclaveAttitude::Neutral)
	{
		return Attitude;
	}

	// A player with no team is neutral both ways: nobody's friend or enemy.
	// A PlayerState without teams counts as having no team.
	const AEnclaveTeamPlayerState* OtherTeamPlayer = Cast<AEnclaveTeamPlayerState>(Other);
	if (!OtherTeamPlayer || TeamId == NoTeam || OtherTeamPlayer->TeamId == NoTeam)
	{
		return EEnclaveAttitude::Neutral;
	}

	return OtherTeamPlayer->TeamId == TeamId ? EEnclaveAttitude::Friendly : EEnclaveAttitude::Hostile;
}

void AEnclaveTeamPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	// Only into another team PlayerState: a solo mode's PlayerState has no team.
	if(AEnclaveTeamPlayerState* TeamPlayerState = Cast<AEnclaveTeamPlayerState>(PlayerState))
	{
		TeamPlayerState->TeamId = TeamId;
	}
}

void AEnclaveTeamPlayerState::OnRep_TeamId()
{
	OnTeamChanged.Broadcast(this, TeamId);
}

void AEnclaveTeamPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AEnclaveTeamPlayerState, TeamId);
}
