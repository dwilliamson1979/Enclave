// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "EnclaveViewComponent.generated.h"

class ACharacter;

/**
 * Handles player view input for an Enclave character.
 *
 * For now this component owns only basic look input. Future view behavior such
 * as ADS, FOV transitions, sensitivity profiles, recoil, and scope presentation
 * can build on this boundary without moving look handling back into the
 * character or player controller.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveViewComponent : public UActorComponent, public IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:
	UEnclaveViewComponent();

	// UActorComponent
	virtual void InitializeComponent() override;

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

private:

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CharacterOwner;

	void RebuildInputFilters();
	bool HandleLookInput(const FEnclaveInputEvent& InputEvent);

	/** Semantic input tag that this component interprets as view/look input. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input"))
	FGameplayTag LookInputTag;

	/** Cached routing filters derived from the configured role tags. */
	FGameplayTagContainer InputFilters;
};
