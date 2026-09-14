// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclaveWieldableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UEnclaveWieldableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for equipment that can receive active item actions.
 *
 * Wieldables do not participate directly in Enclave's input-routing system.
 * The owning EquipmentComponent receives Input.Item events and forwards them
 * to the currently active wieldable.
 *
 * Each wieldable decides what Primary, Secondary, and Tertiary mean.
 */
class ENCLAVE_API IEnclaveWieldableInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equipment|Actions")
	bool HandlePrimaryAction(const FEnclaveInputEvent& InputEvent);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equipment|Actions")
	bool HandleSecondaryAction(const FEnclaveInputEvent& InputEvent);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Equipment|Actions")
	bool HandleTertiaryAction(const FEnclaveInputEvent& InputEvent);
};