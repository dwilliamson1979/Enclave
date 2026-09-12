#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclaveInputConfig.generated.h"

class UInputMappingContext;

/**
 * Defines one Enhanced Input Mapping Context used by an Enclave input config.
 *
 * Priority is passed directly to Enhanced Input when the mapping context
 * is activated for the owning local player.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveInputMappingContextEntry
{
	GENERATED_BODY()

	/**
	 * Enhanced Input mapping context to activate.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	/**
	 * Enhanced Input mapping priority.
	 *
	 * Higher-priority mapping contexts take precedence over lower-priority
	 * contexts when mappings conflict.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;

	bool IsValid() const
	{
		return MappingContext != nullptr;
	}
};

/**
 * Data-driven definition of an Enclave input control layer.
 *
 * Examples:
 * - global player controls
 * - soldier controls
 * - drone controls
 * - vehicle controls
 * - inventory controls
 *
 * An input config defines both the Enhanced Input mapping contexts that
 * should be active and the semantic input bindings that should be routed
 * through the Enclave input framework.
 */
UCLASS(BlueprintType, Const)
class ENCLAVE_API UEnclaveInputConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/**
	 * Mapping contexts that should be activated while this config is active.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FEnclaveInputMappingContextEntry> MappingContexts;

	/**
	 * Enhanced Input actions and the semantic gameplay tags they represent.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TArray<FEnclaveInputActionBinding> ActionBindings;
};