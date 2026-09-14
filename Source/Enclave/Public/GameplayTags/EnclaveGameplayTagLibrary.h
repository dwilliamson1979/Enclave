// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "EnclaveGameplayTagLibrary.generated.h"

UCLASS()
class ENCLAVE_API UEnclaveGameplayTagLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the leaf portion of a gameplay tag.
	 *
	 * Example:
	 * Item.AttachPoint.Wielded.RifleSocket -> RifleSocket
	 */
	UFUNCTION(BlueprintPure, Category = "Enclave|Gameplay Tags")
	static FName GetLeafName(const FGameplayTag& GameplayTag);
};