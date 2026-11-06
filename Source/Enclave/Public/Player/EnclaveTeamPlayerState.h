// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Player/EnclaveMatchPlayerState.h"
#include "EnclaveTeamPlayerState.generated.h"

class AEnclaveTeamPlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnclaveTeamChangedSignature, AEnclaveTeamPlayerState*, PlayerState, uint8, NewTeamId);

/**
 * A player in a team mode (used with AEnclaveTeamMatchGameMode): adds the team.
 * Teams are numbered 0..N-1. NoTeam means "not on a team yet", which in
 * practice is a spectator or a player who hasn't been assigned.
 *
 * Teammates are Friendly to each other; GetAttitudeTowards answers that, so
 * other systems never need to compare teams themselves.
 */
UCLASS()
class ENCLAVE_API AEnclaveTeamPlayerState : public AEnclaveMatchPlayerState
{
	GENERATED_BODY()

public:

	static constexpr uint8 NoTeam = 255;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Match")
	uint8 GetTeamId() const { return TeamId; }
	void SetTeamId(uint8 NewTeamId);

	/** Fires on the server and on clients (OnRep). */
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclaveTeamChangedSignature OnTeamChanged;

	/**
	 * Friendly to a teammate, Hostile to a player on another team. A player with
	 * no team (NoTeam) is Neutral both ways.
	 */
	virtual EEnclaveAttitude GetAttitudeTowards(const APlayerState* Other) const override;

protected:

	virtual void CopyProperties(APlayerState* PlayerState) override;

private:

	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = NoTeam;

	UFUNCTION()
	void OnRep_TeamId();
};
