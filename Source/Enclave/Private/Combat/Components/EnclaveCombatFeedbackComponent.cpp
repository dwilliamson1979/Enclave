// Copyright Lightcarrier Studios

#include "Combat/Components/EnclaveCombatFeedbackComponent.h"

#include "Engine/Engine.h"
#include "Debug/EnclaveDebug.h"

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
		ENCLAVE_NET_LOG(LogEnclaveCombat, Log, GetOwner(), TEXT("[HitMarker] Confirmed hit received on owning client."));
		ENCLAVE_SCREEN(TEXT("HIT"), FColor::Yellow, 1.5f);
	}

	OnWeaponHitConfirmed.Broadcast();
}
