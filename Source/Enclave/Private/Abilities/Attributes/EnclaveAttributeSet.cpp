// Copyright Lightcarrier Studios

#include "Abilities/Attributes/EnclaveAttributeSet.h"

#include "Net/UnrealNetwork.h"

UEnclaveAttributeSet::UEnclaveAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
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
