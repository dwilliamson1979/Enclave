// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "EnclaveGameplayTagPropertyMap.generated.h"

class UAbilitySystemComponent;

/** One Gameplay Tag driving one property on the owner (an anim instance). */
USTRUCT()
struct ENCLAVE_API FEnclaveGameplayTagPropertyBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Gameplay Tags")
	FGameplayTag Tag;

	/**
	 * A property on the owner: bool (tag present), int (tag count), or
	 * float/double (tag count). Blueprint "Float" variables are doubles.
	 */
	UPROPERTY(EditAnywhere, Category = "Gameplay Tags")
	FName PropertyName;
};

/**
 * Drives owner properties from an ASC's tag-change events, like the engine's
 * FGameplayTagBlueprintPropertyMap, but with a lifetime the owner controls.
 *
 * The engine struct has no public way to unbind; it unbinds only in its
 * destructor, at garbage collection. Our ASC lives on the PlayerState and
 * outlives each pawn, so a dead pawn's anim instances stayed bound until GC
 * and every tag change logged a warning. Here the owner calls Unbind when it
 * stops using the ASC (uninitialize, pawn change), and each delegate is tied
 * to the owner, so it never runs for a dead owner either way.
 *
 * Bind applies the tags' current counts immediately, so an owner that binds
 * mid-reload starts in the right state. Don't put it in a container: the
 * delegates refer to this struct by address.
 */
USTRUCT()
struct ENCLAVE_API FEnclaveGameplayTagPropertyMap
{
	GENERATED_BODY()

	FEnclaveGameplayTagPropertyMap() = default;
	~FEnclaveGameplayTagPropertyMap();

	/** Copies the configuration only, never a live binding (class defaults are copied into each instance). */
	FEnclaveGameplayTagPropertyMap(const FEnclaveGameplayTagPropertyMap& Other);
	FEnclaveGameplayTagPropertyMap& operator=(const FEnclaveGameplayTagPropertyMap& Other);

	/** Binds every valid entry to ASC and applies current counts. Unbinds from any previous ASC first. */
	void Bind(UObject* Owner, UAbilitySystemComponent* ASC);

	/** Removes every delegate from the bound ASC. Safe to call when not bound. */
	void Unbind();

	UAbilitySystemComponent* GetBoundAbilitySystemComponent() const { return BoundAbilitySystemComponent.Get(); }

#if WITH_EDITOR
	/** Reports entries with no tag, or a property that is missing on OwnerClass or of an unsupported type. */
	void ValidateBindings(const UClass* OwnerClass, class FDataValidationContext& Context) const;
#endif

private:
	struct FActiveBinding
	{
		FGameplayTag Tag;
		EGameplayTagEventType::Type EventType = EGameplayTagEventType::NewOrRemoved;
		FDelegateHandle Handle;
	};

	/** The property named PropertyName on OwnerClass, if it is a supported type. */
	static FProperty* FindSupportedProperty(const UClass* OwnerClass, FName PropertyName);

	static void SetPropertyFromCount(UObject* Owner, const FProperty* Property, int32 Count);

	/** Tag -> property entries, configured in the owner's class defaults. */
	UPROPERTY(EditAnywhere, Category = "Gameplay Tags")
	TArray<FEnclaveGameplayTagPropertyBinding> Bindings;

	TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystemComponent;
	TArray<FActiveBinding> ActiveBindings;
};
