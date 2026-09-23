// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "EnclaveGameplayAbility.generated.h"

/**
 * Project base class for all Enclave Gameplay Abilities.
 *
 * Deliberately thin for now - it exists so every ability shares one place to
 * add common behavior later (cost/cooldown conventions, a shared way to read
 * the source item, etc.) rather than needing a mechanical find-and-replace
 * across every ability subclass once that need shows up.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UEnclaveGameplayAbility();

protected:
	/**
	 * Resolves whatever triggered this ability's activation (GetCurrentSourceObject(),
	 * the FGameplayAbilitySpec::SourceObject set when the ability was
	 * granted) as T - an actor class, or a UInterface this project defines.
	 * Deliberately generic rather than "the weapon" specifically: a triggering
	 * source could be a wielded weapon today, and a passive inventory item or
	 * deployed piece of equipment later - this class has no opinion on which,
	 * and a leaf ability shouldn't need one either. A leaf ability resolves
	 * straight to the interface it actually needs (e.g.
	 * GetTriggeringSourceAs<IEnclaveFireableInterface>() in UEnclaveGA_Fire)
	 * rather than this class enumerating every kind of triggering source up
	 * front. Returns nullptr if nothing was granted as a source object, or it
	 * doesn't implement/derive from T.
	 */
	template<class T>
	T* GetTriggeringSourceAs() const { return Cast<T>(GetCurrentSourceObject()); }
};