// Copyright Lightcarrier Studios

#include "Abilities/Attributes/EnclaveAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UEnclaveAttributeSet::UEnclaveAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
}

void UEnclaveAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Guards any direct write to Health (an Initter, a non-Effect setter) -
	// separate from, and not a substitute for, PostGameplayEffectExecute
	// below, which guards the result of an applied Gameplay Effect.
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UEnclaveAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// Clamps the result of a Gameplay Effect (the Fire ability's damage
	// effect, going forward). Phase 2 does not yet add a "died" event/tag
	// here on reaching 0 - that belongs with the game mode's respawn/death
	// handling, not the Attribute Set itself.
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
}

void UEnclaveAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UEnclaveAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UEnclaveAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UEnclaveAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnclaveAttributeSet, Health, OldHealth);
}

void UEnclaveAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UEnclaveAttributeSet, MaxHealth, OldMaxHealth);
}
