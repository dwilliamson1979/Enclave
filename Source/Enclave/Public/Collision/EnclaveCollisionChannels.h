// Copyright Lightcarrier Studios

#pragma once

#include "Engine/EngineTypes.h"
#include "EnclaveCollisionChannels.generated.h"

/**
 * Project-defined trace channels. The underlying slots (GameTraceChannel1,
 * etc.) and their display names/defaults are configured in Project
 * Settings -> Collision -> Trace Channels (serialized to
 * Config/DefaultEngine.ini) - these constants just give the raw enum
 * values readable names in C++.
 */
constexpr ECollisionChannel ECC_Weapon = ECC_GameTraceChannel1;

/**
 * Server-authoritative representation a character exposes to Weapon traces.
 *
 * Capsule is the forgiving model. SkeletalHitboxes uses the third-person
 * skeletal mesh's Physics Asset so FHitResult::BoneName can identify the
 * struck body region. The active value is a match/server rule owned by
 * AEnclaveGameMode, never by an individual weapon.
 */
UENUM(BlueprintType)
enum class EEnclaveWeaponHitDetectionMode : uint8
{
	Capsule UMETA(DisplayName = "Capsule"),
	SkeletalHitboxes UMETA(DisplayName = "Skeletal Hitboxes")
};

/** Stable diagnostic name for logging/debug traces. */
ENCLAVE_API const TCHAR* GetEnclaveWeaponHitDetectionModeName(EEnclaveWeaponHitDetectionMode Mode);
