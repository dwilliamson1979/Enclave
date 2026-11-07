// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveDamagePolicyInterface.generated.h"

class AActor;
struct FEnclaveDamageRequest;

/** What happens to a hit. */
UENUM(BlueprintType)
enum class EEnclaveHitResponse : uint8
{
	/** No damage and no hit feedback. */
	Ignore,
	/** Hit feedback (hit marker, damage number), but health doesn't change. */
	Simulate,
	/** Real damage. */
	Apply
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UEnclaveDamagePolicyInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * The game's damage rules, asked by UEnclaveDamageStatics::ApplyDamage for
 * every damage request. Implemented by the game mode, so combat code depends
 * on this interface, never on a game mode class. A game mode without it
 * applies all damage unchanged (the test maps).
 */
class ENCLAVE_API IEnclaveDamagePolicyInterface
{
	GENERATED_BODY()

public:
	/**
	 * Server only. Target is the actor about to receive InOutRequest. Decides the
	 * response, and may change the request (e.g. scale friendly-fire damage).
	 */
	virtual EEnclaveHitResponse ResolveDamage(const AActor* Target, FEnclaveDamageRequest& InOutRequest) const = 0;
};
