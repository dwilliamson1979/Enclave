// Copyright Lightcarrier Studios

#include "Combat/Components/EnclaveCombatFeedbackComponent.h"

#include "Engine/Engine.h"

static TAutoConsoleVariable<bool> CVarDebugHitMarker(
	TEXT("Enclave.Debug.HitMarker"),
	false,
	TEXT("Log and print an on-screen message whenever this client receives a confirmed weapon hit notification."),
	ECVF_Cheat);

UEnclaveCombatFeedbackComponent::UEnclaveCombatFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEnclaveCombatFeedbackComponent::NotifyWeaponHitConfirmed()
{
	// Not asserted to be server-only: the interface only promises the
	// notification reaches the owning client, not that the caller is
	// server code. In practice it currently is (EnclaveGA_Fire's trace is
	// authority-gated), so this always has a network hop to make.
	Client_NotifyWeaponHitConfirmed();
}

void UEnclaveCombatFeedbackComponent::Client_NotifyWeaponHitConfirmed_Implementation()
{
	if (CVarDebugHitMarker.GetValueOnGameThread())
	{
		UE_LOG(LogTemp, Warning, TEXT("[HitMarker] Confirmed hit received on owning client."));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 1.5f, FColor::Yellow, TEXT("HIT"));
		}
	}

	OnWeaponHitConfirmed.Broadcast();
}
