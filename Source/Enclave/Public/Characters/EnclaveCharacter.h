// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Input/Interfaces/EnclaveInputReceiverInterface.h"
#include "Input/Interfaces/EnclaveInputConfigProviderInterface.h"
#include "EnclaveCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
//class USceneComponent;
class UEnclaveInputConfig;
class UEnclaveInputRouterComponent;
class UEnclaveCharacterMovementComponent;
class UEnclaveViewComponent;
class UEnclaveCombatComponent;
class UEnclaveEquipmentComponent;
class UEnclaveItemAttachmentDefinition;

UCLASS()
class ENCLAVE_API AEnclaveCharacter : public ACharacter, public IEnclaveInputReceiverInterface, public IEnclaveInputConfigProviderInterface
{
	GENERATED_BODY()

public:
	AEnclaveCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


	// Interface implementations

	//IInputReceiverInterface
	virtual void ProcessInput(const FEnclaveInputEvent& InputEvent) override;

	//IEnclaveInputConfigProviderInterface
	virtual const UEnclaveInputConfig* GetInputConfig() const override;

	UEnclaveCharacterMovementComponent* GetEnclaveCharacterMovement() const;
	
	const UEnclaveItemAttachmentDefinition* GetItemAttachmentDefinition() const;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

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


	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveCombatComponent> CombatComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveEquipmentComponent> EquipmentComp = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveItemAttachmentDefinition> ItemAttachmentDefinition = nullptr;

};