// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "EnclaveCharacterMovementComponent.generated.h"

/**
 * Enclave-specific character locomotion component.
 *
 * Extends Unreal's CharacterMovementComponent rather than replacing its
 * movement/networking model. It also participates in Enclave's semantic input
 * routing for locomotion-related avatar input.
 */
UCLASS()
class ENCLAVE_API UEnclaveCharacterMovementComponent : public UCharacterMovementComponent, public IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:
	UEnclaveCharacterMovementComponent();

	// UActorComponent
	virtual void InitializeComponent() override;

	// IEnclaveInputHandlerInterface
	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

private:
	void RebuildInputFilters();
	bool HandleMoveInput(const FEnclaveInputEvent& InputEvent);
	bool HandleJumpInput(const FEnclaveInputEvent& InputEvent);
	bool HandleCrouchInput(const FEnclaveInputEvent& InputEvent);

	/** Semantic input tag that this component interprets as character movement. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input"))
	FGameplayTag MoveInputTag;

	/** Semantic input tag that this component interprets as jump. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input"))
	FGameplayTag JumpInputTag;

	/** Semantic input tag that this component interprets as crouch. */
	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input"))
	FGameplayTag CrouchInputTag;

	/**
	 * Cached routing filters derived from the configured role tags.
	 *
	 * This is intentionally not editor-configurable so the role-tag properties
	 * remain the single source of truth.
	 */
	FGameplayTagContainer InputFilters;
};
