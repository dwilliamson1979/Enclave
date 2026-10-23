#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnclaveSSRPatrolComponent.generated.h"

/** Opt-in development test target: authority drives a possessed Character back and forth.
 * Attach to a duplicate Enclave character Blueprint placed in a test map.
 * Uses CharacterMovement (not teleport/SetActorLocation) so replication smoothing is exercised.
 */
UCLASS(ClassGroup=(EnclaveDebug), meta=(BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveSSRPatrolComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEnclaveSSRPatrolComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Half-travel distance measured from the initial position, in centimeters. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSR Preflight", meta=(ClampMin="100.0"))
    float HalfDistanceCm = 350.f;

    /** Pause briefly before reversing, producing useful stop/start smoothing samples. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SSR Preflight", meta=(ClampMin="0.0"))
    float EndpointPauseSeconds = 0.25f;

private:
    FVector StartLocation = FVector::ZeroVector;
    FVector TravelDirection = FVector::ForwardVector;
    float PauseRemaining = 0.f;
    int32 DirectionSign = 1;
    bool bReportedMissingController = false;
};
