// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Interfaces/EnclaveHitFeedbackInterface.h"
#include "EnclaveCombatFeedbackComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnclaveOnWeaponHitConfirmed);

/**
 * Owns local-player combat feedback - a hit marker today, with room to grow
 * into a damage-taken direction indicator, a kill confirmation, etc.
 * without adding more concrete cross-domain dependencies elsewhere: callers
 * depend on IEnclaveHitFeedbackInterface, never on this class directly.
 *
 * Lives on AEnclavePlayerController rather than the Pawn. Mirrors the
 * reasoning behind AEnclavePlayerState hosting persistent gameplay state
 * independent of the currently possessed Pawn - this hosts client-only
 * presentation state the same way, and the PlayerController, not the Pawn,
 * is the actor that only ever exists for its own owning connection.
 *
 * A UActorComponent added in a replicated actor's constructor replicates by
 * default (see SetIsReplicatedByDefault below), which is what makes the
 * Client RPC valid with no further wiring.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveCombatFeedbackComponent : public UActorComponent, public IEnclaveHitFeedbackInterface
{
	GENERATED_BODY()

public:
	UEnclaveCombatFeedbackComponent();

	// IEnclaveHitFeedbackInterface
	virtual void NotifyWeaponHitConfirmed() override;

	/** Bind here (Blueprint or C++) to react to a confirmed hit - flash a crosshair marker, play a sound, etc. Only ever broadcasts on the owning client. */
	UPROPERTY(BlueprintAssignable, Category = "Combat|Feedback")
	FEnclaveOnWeaponHitConfirmed OnWeaponHitConfirmed;

private:
	UFUNCTION(Client, Reliable)
	void Client_NotifyWeaponHitConfirmed();
};
