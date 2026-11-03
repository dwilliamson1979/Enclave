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

bool AEnclaveTeamPlayerState::IsHostileTo(const APlayerState* Other) const
{
	if (!Super::IsHostileTo(Other))
	{
		return false;
	}

	const AEnclaveTeamPlayerState* OtherTeamPlayer = Cast<AEnclaveTeamPlayerState>(Other);
	const bool bSameTeam = OtherTeamPlayer && TeamId != NoTeam && OtherTeamPlayer->TeamId == TeamId;
	return !bSameTeam;
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
