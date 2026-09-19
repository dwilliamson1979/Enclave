// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "EnclavePlayerState.generated.h"

class UAbilitySystemComponent;
class UEnclaveAttributeSet;

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
	AEnclavePlayerState();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UEnclaveAttributeSet* GetEnclaveAttributeSet() const { return AttributeSet; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UEnclaveAttributeSet> AttributeSet;
};
