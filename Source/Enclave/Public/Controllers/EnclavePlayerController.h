#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclavePlayerController.generated.h"

class UEnhancedInputComponent;
class UEnclaveInputConfig;
class IEnclaveInputReceiverInterface;
class IEnclaveInputConfigProviderInterface;

UCLASS()
class ENCLAVE_API AEnclavePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AEnclavePlayerController();

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	/**
	 * Determines whether a semantic input event should be forwarded to gameplay.
	 *
	 * Override this for controller-level gating such as menus, possession
	 * transitions, global input suppression, or other player-wide policies.
	 */
	virtual bool CanProcessInput(const FEnclaveInputEvent& InputEvent) const;

private:

	void HandleInput(const FInputActionValue& Value, ETriggerEvent TriggerEvent, FGameplayTag InputTag);

	void RefreshPossessedPawnInputState();

	void ActivateInputConfig(const UEnclaveInputConfig* InputConfig);
	void DeactivateInputConfig(const UEnclaveInputConfig* InputConfig);

	void AddInputConfigMappingContexts(const UEnclaveInputConfig* InputConfig);
	void RemoveInputConfigMappingContexts(const UEnclaveInputConfig* InputConfig);

	void BindInputConfigActions(UEnhancedInputComponent& EnhancedInput, const UEnclaveInputConfig* InputConfig);
	void ClearInputBindings();

	IEnclaveInputReceiverInterface* CachedInputReceiver = nullptr;
	IEnclaveInputConfigProviderInterface* CachedInputConfigProvider = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<const UEnclaveInputConfig> ActiveAvatarInputConfig = nullptr;

	TArray<uint32> ActiveInputBindingHandles;
};