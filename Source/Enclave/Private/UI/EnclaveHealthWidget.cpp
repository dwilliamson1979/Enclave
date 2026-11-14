// Copyright Lightcarrier Studios

#include "UI/EnclaveHealthWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Health/Components/EnclaveHealthComponent.h"

void UEnclaveHealthWidget::NativeDestruct()
{
	BindHealthComponent(nullptr);

	Super::NativeDestruct();
}

void UEnclaveHealthWidget::OnPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	BindHealthComponent(UEnclaveHealthComponent::FindHealthComponent(NewPawn));

	Super::OnPawnChanged(OldPawn, NewPawn);
}

void UEnclaveHealthWidget::BindHealthComponent(UEnclaveHealthComponent* NewHealthComponent)
{
	if (UEnclaveHealthComponent* OldHealthComponent = HealthComponent.Get())
	{
		OldHealthComponent->OnHealthAvailable.RemoveDynamic(this, &UEnclaveHealthWidget::HandleHealthAvailable);
		OldHealthComponent->OnHealthChanged.RemoveDynamic(this, &UEnclaveHealthWidget::HandleHealthChanged);
		OldHealthComponent->OnMaxHealthChanged.RemoveDynamic(this, &UEnclaveHealthWidget::HandleMaxHealthChanged);
	}

	HealthComponent = NewHealthComponent;
	if (NewHealthComponent)
	{
		NewHealthComponent->OnHealthAvailable.AddUniqueDynamic(this, &UEnclaveHealthWidget::HandleHealthAvailable);
		NewHealthComponent->OnHealthChanged.AddUniqueDynamic(this, &UEnclaveHealthWidget::HandleHealthChanged);
		NewHealthComponent->OnMaxHealthChanged.AddUniqueDynamic(this, &UEnclaveHealthWidget::HandleMaxHealthChanged);
	}

	// Show what is already there; the events only cover what follows.
	Refresh();
}

void UEnclaveHealthWidget::Refresh()
{
	const UEnclaveHealthComponent* Health = HealthComponent.Get();
	if (!Health || !Health->IsHealthAvailable())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (HealthBar)
	{
		HealthBar->SetPercent(Health->GetHealthNormalized());
	}

	if (HealthText)
	{
		// Round up: a sliver of health left shows 1, never 0 while alive.
		HealthText->SetText(FText::AsNumber(FMath::CeilToInt(Health->GetHealth())));
	}
}

void UEnclaveHealthWidget::HandleHealthAvailable(UEnclaveHealthComponent* InHealthComponent)
{
	Refresh();
}

void UEnclaveHealthWidget::HandleHealthChanged(UEnclaveHealthComponent* InHealthComponent, float OldHealth, float NewHealth)
{
	Refresh();
}

void UEnclaveHealthWidget::HandleMaxHealthChanged(UEnclaveHealthComponent* InHealthComponent, float OldMaxHealth, float NewMaxHealth)
{
	Refresh();
}
