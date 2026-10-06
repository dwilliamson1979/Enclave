// Copyright Lightcarrier Studios

#include "Collision/EnclaveCollisionChannels.h"

const TCHAR* GetEnclaveWeaponHitDetectionModeName(EEnclaveWeaponHitDetectionMode Mode)
{
	switch (Mode)
	{
	case EEnclaveWeaponHitDetectionMode::SkeletalHitboxes:
		return TEXT("SkeletalHitboxes");

	case EEnclaveWeaponHitDetectionMode::Capsule:
	default:
		return TEXT("Capsule");
	}
}
