// Copyright Lightcarrier Studios

#include "Debug/EnclaveSSRPatrolComponent.h"

#include "Debug/EnclaveDebug.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"

#if ENCLAVE_INCLUDE_DEBUG_CODE
static TAutoConsoleVariable<int32> CVarSSRPatrol(
	TEXT("Enclave.Debug.SSRPatrol"), 0,
	TEXT("Enable authority-driven movement of EnclaveSSRPatrolComponent test characters (0=off, 1=on). Development builds only."),
	ECVF_Cheat);
#endif

UEnclaveSSRPatrolComponent::UEnclaveSSRPatrolComponent()
{
	// This component is a test rig, not production gameplay. Keep live CVar
	// toggling in development builds, but never tick it in Shipping or Test.
	PrimaryComponentTick.bCanEverTick = ENCLAVE_INCLUDE_DEBUG_CODE;
}

void UEnclaveSSRPatrolComponent::BeginPlay()
{
	Super::BeginPlay();

#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		StartLocation = Character->GetActorLocation();
		TravelDirection = Character->GetActorForwardVector().GetSafeNormal2D();
		if (TravelDirection.IsNearlyZero())
		{
			TravelDirection = FVector::ForwardVector;
		}
	}
#endif
}

void UEnclaveSSRPatrolComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if ENCLAVE_INCLUDE_DEBUG_CODE
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!IsValid(Character) || !Character->HasAuthority() || CVarSSRPatrol.GetValueOnGameThread() == 0)
	{
		return;
	}

	// The test character must have a server-side AI controller.
	if (!Character->GetController())
	{
		if (!bReportedMissingController)
		{
			ENCLAVE_LOG(LogEnclaveCharacter, Warning,
				"SSRPatrol %s has no controller: set Auto Possess AI to Placed in World or Spawned in the test Blueprint.",
				*Character->GetName());
			bReportedMissingController = true;
		}
		return;
	}
	bReportedMissingController = false;

	if (PauseRemaining > 0.0f)
	{
		PauseRemaining = FMath::Max(0.0f, PauseRemaining - DeltaTime);
		return;
	}

	const float Offset = FVector::DotProduct(Character->GetActorLocation() - StartLocation, TravelDirection);
	if ((DirectionSign > 0 && Offset >= HalfDistanceCm) ||
		(DirectionSign < 0 && Offset <= -HalfDistanceCm))
	{
		DirectionSign *= -1;
		PauseRemaining = EndpointPauseSeconds;
		return;
	}

	Character->AddMovementInput(TravelDirection * static_cast<float>(DirectionSign), 1.0f);
#endif
}
