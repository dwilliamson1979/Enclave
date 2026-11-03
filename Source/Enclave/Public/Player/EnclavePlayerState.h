// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "EnclavePlayerState.generated.h"

class UAbilitySystemComponent;
class UEnclaveAttributeSet;

/**
 * Root PlayerState for every Enclave map (the SSR test range uses it directly).
 *
 * Owns the player's persistent Gameplay Ability System state for the whole
 * match, independent of whichever Pawn is currently possessed.
 *
 * This is the "Owner Actor" in GAS terms - it does not change across a
 * respawn. The currently possessed AEnclaveCharacter is the "Avatar Actor"
 * and is expected to change; AEnclaveCharacter re-points the ASC at itself
 * as avatar via InitAbilityActorInfo in PossessedBy/OnRep_PlayerState rather
 * than owning its own ASC.
 *
 * Like the GameState, the PlayerState tree mirrors the GameMode tree, and each
 * GameMode sets PlayerStateClass to its own level:
 *
 *   AEnclavePlayerState            AEnclaveGameMode            every map: ASC, hostility
 *     AEnclaveMatchPlayerState       AEnclaveMatchGameMode       kills, deaths (solo modes use it directly)
 *       AEnclaveTeamPlayerState        AEnclaveTeamMatchGameMode   team
 */
UCLASS()
class ENCLAVE_API AEnclavePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AEnclavePlayerState();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UEnclaveAttributeSet* GetEnclaveAttributeSet() const { return AttributeSet; }

	/**
	 * Whether Other is this player's enemy. Every map has enemies, so the
	 * question lives here; how it is answered depends on the mode. Default:
	 * anyone else. AEnclaveTeamPlayerState narrows it to other teams.
	 *
	 * Ask this instead of comparing teams, so friendly fire, spawn selection,
	 * friend/enemy colors and bots work in every mode. PlayerStates replicate,
	 * so it gives the same answer on the server and on clients.
	 */
	UFUNCTION(BlueprintPure, Category = "Match")
	virtual bool IsHostileTo(const APlayerState* Other) const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UEnclaveAttributeSet> AttributeSet;
};
