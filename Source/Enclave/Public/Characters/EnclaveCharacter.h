// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Input/Interfaces/EnclaveInputReceiverInterface.h"
#include "Equipment/Interfaces/EnclaveItemWielderInterface.h"
#include "Input/Interfaces/EnclaveInputConfigProviderInterface.h"
#include "EnclaveCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UEnclaveInputConfig;
class UEnclaveInputRouterComponent;
class UEnclaveCharacterMovementComponent;
class UEnclaveViewComponent;
class UEnclaveEquipmentComponent;
class UEnclaveInventoryComponent;
class UEnclaveItemAttachmentDefinition;
class AEnclaveItem;
class UAbilitySystemComponent;

UCLASS()
class ENCLAVE_API AEnclaveCharacter : public ACharacter, public IEnclaveInputReceiverInterface, public IEnclaveInputConfigProviderInterface, public IEnclaveItemWielderInterface, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AEnclaveCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;

	/**
	 * Client-side counterpart to PossessedBy: fires when the replicated
	 * PlayerState reference lands on a client, which happens on a different
	 * timeline than server-side possession. Both paths must independently
	 * (re)point the ASC's avatar at this Pawn - this is the single most
	 * common GAS setup bug (abilities silently not working on one side of
	 * the connection because actor info was only initialized on the other).
	 */
	virtual void OnRep_PlayerState() override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Interface implementations

	// IEnclaveInputReceiverInterface
	virtual void ProcessInput(const FEnclaveInputEvent& InputEvent) override;

	// IEnclaveInputConfigProviderInterface
	virtual const UEnclaveInputConfig* GetInputConfig() const override;

	// IEnclaveItemWielderInterface
	virtual bool ResolveWieldedItemAttachmentTargets(const FGameplayTag& ItemType, FEnclaveWieldedItemAttachmentTargets& OutTargets) const override;

	UEnclaveCharacterMovementComponent* GetEnclaveCharacterMovement() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:
	/** Shared by PossessedBy (server) and OnRep_PlayerState (client) - both must call this independently. */
	void InitializeAbilityActorInfo();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArmComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> CameraComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveViewComponent> ViewComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveInputRouterComponent> InputRouterComp = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveInputConfig> InputConfig = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveInventoryComponent> InventoryComp = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<AEnclaveItem>> DefaultItemClasses;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveEquipmentComponent> EquipmentComp = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveItemAttachmentDefinition> ItemAttachmentDefinition = nullptr;
};
