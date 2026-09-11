// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "EnclaveCharacter.generated.h"

UCLASS()
class ENCLAVE_API AEnclaveCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AEnclaveCharacter();

	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

private:

};