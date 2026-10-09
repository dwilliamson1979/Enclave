// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Input/Interfaces/EnclaveInputReceiverInterface.h"
#include "Input/Interfaces/EnclaveInputConfigProviderInterface.h"
#include "Input/Interfaces/EnclaveAvatarInputPolicyInterface.h"
#include "Animation/Interfaces/EnclaveAnimDataProviderInterface.h"
#include "EnclaveCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UEnclaveInputConfig;
class UEnclaveInputRouterComponent;
class UEnclaveCharacterMovementComponent;
class UEnclaveViewComponent;
class UEnclaveEquipmentComponent;
class UEnclaveInventoryComponent;
class AEnclaveItem;
class UEnclaveAvatarPresentationComponent;
class UAbilitySystemComponent;
class UEnclaveGASHealthComponent;
class UEnclaveAvatarLifecycleComponent;

UCLASS()
class ENCLAVE_API AEnclaveCharacter : public ACharacter, public IEnclaveInputReceiverInterface, public IEnclaveInputConfigProviderInterface, public IEnclaveAvatarInputPolicyInterface, public IAbilitySystemInterface, public IEnclaveAnimDataProviderInterface
{
	GENERATED_BODY()

public:
	AEnclaveCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

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

	// IEnclaveAvatarInputPolicyInterface
	virtual bool CanProcessAvatarInput(const FEnclaveInputEvent& InputEvent) const override;

	// IEnclaveAnimDataProviderInterface
	virtual FEnclaveAnimData GetAnimData() const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Shared by PossessedBy (server) and OnRep_PlayerState (client) - both must call this independently. */
	void InitializeAbilityActorInfo();

	/** Initializes all avatar components that opt into the ASC-consumer contract. */
	void InitializeAbilitySystemConsumers(UAbilitySystemComponent* AbilitySystemComponent);

	/** Removes ASC bindings from all avatar components currently consuming it. */
	void UninitializeAbilitySystemConsumers();

	/** Applies persistent-ASC effect lifetime policy exactly once before this Pawn binds as avatar. */
	void InitializeAvatarEffectScopeIfNeeded(UAbilitySystemComponent* AbilitySystemComponent);

	/** Authority only: exposes exactly one character representation to the Weapon trace channel for the selected match hit-detection mode. */
	void ApplyWeaponHitDetectionCollisionPolicy();

	// BlueprintReadOnly so BP_EnclaveCharacter's graph can reach it (e.g. to
	// test Link Anim Class Layers before the equip-driven C++ path exists).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh = nullptr;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveAvatarPresentationComponent> AvatarPresentationComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveGASHealthComponent> HealthComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveAvatarLifecycleComponent> LifecycleComp = nullptr;

	/** Non-owning cache of the PlayerState-owned ASC currently initialized for this avatar. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	/**
	 * Remembers which persistent ASC has already had its previous avatar-lifetime state
	 * swept for this Pawn. It deliberately survives UnPossessed so re-binding
	 * this same Pawn to the same ASC does not clear valid effects or revive it.
	 */
	TWeakObjectPtr<UAbilitySystemComponent> AvatarEffectScopeInitializedAbilitySystem;

};
