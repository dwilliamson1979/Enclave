// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UI/EnclaveHUDWidget.h"
#include "EnclaveHealthWidget.generated.h"

class UEnclaveHealthComponent;
class UProgressBar;
class UTextBlock;

/**
 * The local pawn's health: a bar, and optionally the number. Reads the pawn's
 * UEnclaveHealthComponent (any health model) and updates on its events; hidden
 * while there is no pawn (dead, spectating) or its health isn't available yet.
 *
 * The Widget Blueprint must contain a ProgressBar named HealthBar, and may
 * contain a TextBlock named HealthText.
 */
UCLASS(Abstract)
class ENCLAVE_API UEnclaveHealthWidget : public UEnclaveHUDWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeDestruct() override;
	virtual void OnPawnChanged(APawn* OldPawn, APawn* NewPawn) override;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidget))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, Category = "HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

private:
	void BindHealthComponent(UEnclaveHealthComponent* NewHealthComponent);
	void Refresh();

	UFUNCTION()
	void HandleHealthAvailable(UEnclaveHealthComponent* InHealthComponent);

	UFUNCTION()
	void HandleHealthChanged(UEnclaveHealthComponent* InHealthComponent, float OldHealth, float NewHealth);

	UFUNCTION()
	void HandleMaxHealthChanged(UEnclaveHealthComponent* InHealthComponent, float OldMaxHealth, float NewMaxHealth);

	TWeakObjectPtr<UEnclaveHealthComponent> HealthComponent;
};
