// Copyright Lightcarrier Studios

#pragma once

#include "Engine/EngineTypes.h"

/**
 * Project-defined trace channels. The underlying slots (GameTraceChannel1,
 * etc.) and their display names/defaults are configured in Project
 * Settings -> Collision -> Trace Channels (serialized to
 * Config/DefaultEngine.ini) - these constants just give the raw enum
 * values readable names in C++.
 */
constexpr ECollisionChannel ECC_Weapon = ECC_GameTraceChannel1;