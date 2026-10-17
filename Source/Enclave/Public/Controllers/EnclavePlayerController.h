#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Input/Core/EnclaveInputTypes.h"
#include "EnclavePlayerController.generated.h"

class UEnhancedInputComponent;
class UEnclaveInputConfig;
class IEnclaveInputReceiverInterface;
class IEnclaveInputConfigProviderInterface;
class UEnclaveCombatFeedbackComponent;

UCLASS()
class ENCLAVE_API AEnclavePlayerController : public APlayerController
{
	GENERATED_BODY()

public:

	AEnclavePlayerController();

	/**
	 * Development-only lifecycle test. Recreates this controller's pawn while
	 * preserving the PlayerState-owned ASC so stale item specs/effects are easy
	 * to detect across repeated respawns.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugRespawn();

	/**
	 * Development-only inventory-removal test. Removes and destroys the currently
	 * wielded ordinary inventory item on authority, exercising Equipment fallback.
	 * The persistent default item is never removable through this command.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugRemoveWieldedItem();

	/**
	 * Development-only slot clearing test. Resolves a registered Gameplay Tag,
	 * then removes that ordinary slot's Equipment assignment on authority.
	 * Does not remove the item from Inventory or destroy it. Reserved Default
	 * and Stowed slots are intentionally excluded from this testing command.
	 */
	UFUNCTION(Exec)
	void EnclaveDebugClearSlot(const FString& SlotTagName);

protected:

	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(APawn* P) override;
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

	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugRespawn();

	void PerformDebugRespawn();

	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugRemoveWieldedItem();

	void PerformDebugRemoveWieldedItem();

	UFUNCTION(Server, Reliable)
	void ServerEnclaveDebugClearSlot(FGameplayTag SlotTag);

	void PerformDebugClearSlot(FGameplayTag SlotTag);

	UFUNCTION(Client, Reliable)
	void ClientEnclaveDebugClearSlotResult(bool bSucceeded, const FString& ResultMessage);

	void HandleInput(const FInputActionValue& Value, ETriggerEvent TriggerEvent, FGameplayTag InputTag, bool bRoutesReleaseEvents);

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

	/**
	 * Hosts local-player combat feedback (hit marker today). Implements
	 * IEnclaveHitFeedbackInterface - callers (e.g. UEnclaveGA_Fire) find it
	 * via FindComponentByInterface rather than depending on this class, the
	 * same idiom UEnclaveEquipmentComponent uses to find its item source.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveCombatFeedbackComponent> CombatFeedbackComp = nullptr;
};