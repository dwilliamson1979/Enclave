// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Input/Interfaces/EnclaveInputHandlerInterface.h"
#include "Equipment/Focus/EnclaveFocusProfile.h"
#include "EnclaveViewComponent.generated.h"

class ACharacter;
class AEnclaveItem;
class UCameraComponent;

/**
 * Local view/look presentation. Input is only Look; item actions reach GAS
 * through Equipment. Focus presentation arrives from AvatarPresentation,
 * with a resolved profile. This component never activates abilities.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveViewComponent : public UActorComponent, public IEnclaveInputHandlerInterface
{
	GENERATED_BODY()

public:
	UEnclaveViewComponent();

	virtual void InitializeComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual const FGameplayTagContainer& GetInputFilters() const override;
	virtual bool HandleInput(const FEnclaveInputEvent& InputEvent) override;

	/** Explicit camera binding; an avatar may have more than one camera. */
	void SetViewCamera(UCameraComponent* InCamera);

	/** Called by presentation only, with already-resolved item settings. */
	void BeginFocus(AEnclaveItem* SourceItem, const FEnclaveFocusViewSettings& Settings);
	void EndFocus(AEnclaveItem* SourceItem);

	/** Teardown/re-possession safety net; no stale Focus can retain camera changes. */
	void ResetFocus(bool bImmediate = false);

private:
	void RebuildInputFilters();
	bool HandleLookInput(const FEnclaveInputEvent& InputEvent);
	void BeginViewBlend(float InFOV, float InSensitivity, float Duration);

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CharacterOwner;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> ViewCamera;

	UPROPERTY(EditDefaultsOnly, Category = "Input|Tags", meta = (Categories = "Input"))
	FGameplayTag LookInputTag;

	FGameplayTagContainer InputFilters;
	TWeakObjectPtr<AEnclaveItem> FocusSourceItem;

	bool bFocusActive = false;
	bool bHasBaseFOV = false;
	float BaseFOV = 90.0f;
	float FocusTransitionDuration = 0.2f;
	float CurrentSensitivityMultiplier = 1.0f;
	float BlendStartSensitivity = 1.0f;
	float BlendTargetSensitivity = 1.0f;
	float BlendStartFOV = 90.0f;
	float BlendTargetFOV = 90.0f;
	float BlendDuration = 0.0f;
	float BlendElapsed = 0.0f;
};
