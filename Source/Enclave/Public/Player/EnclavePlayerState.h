// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "EnclavePlayerState.generated.h"

class UAbilitySystemComponent;
class UEnclaveAttributeSet;
class AEnclavePlayerState;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnclaveTeamChangedSignature, AEnclavePlayerState*, PlayerState, uint8, NewTeamId);

/**
 * Owns the player's persistent Gameplay Ability System state for the whole
 * match, independent of whichever Pawn is currently possessed.
 *
 * This is the "Owner Actor" in GAS terms - it does not change across a
 * respawn. The currently possessed AEnclaveCharacter is the "Avatar Actor"
 * and is expected to change; AEnclaveCharacter re-points the ASC at itself
 * as avatar via InitAbilityActorInfo in PossessedBy/OnRep_PlayerState rather
 * than owning its own ASC.
 */
UCLASS()
class ENCLAVE_API AEnclavePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	//statics
	static constexpr uint8 NoTeam = 255;

	AEnclavePlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UEnclaveAttributeSet* GetEnclaveAttributeSet() const { return AttributeSet; }

	UFUNCTION(BlueprintPure, Category = "Match")
	uint8 GetTeamId() const { return TeamId; }
	void SetTeamId(uint8 NewTeamId);

	void AddKill();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetKills() const { return Kills; }

	void AddDeath();
	UFUNCTION(BlueprintPure, Category = "Match")
	int32 GetDeaths() const { return Deaths; }

	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnEnclaveTeamChangedSignature OnTeamChanged;

	virtual void Reset() override;

protected:
	
	virtual void CopyProperties(APlayerState* PlayerState) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UEnclaveAttributeSet> AttributeSet;

	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = NoTeam;

	UFUNCTION()
	void OnRep_TeamId();
};
