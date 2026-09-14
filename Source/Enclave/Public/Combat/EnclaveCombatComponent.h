// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnclaveCombatComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ENCLAVE_API UEnclaveCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	

	UEnclaveCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Initiate_CycleWeapon();

	void Initiate_FireWeapon_Pressed();
	void Initiate_FireWeapon_Released();

	void Initiate_ReloadWeapon();
		
};