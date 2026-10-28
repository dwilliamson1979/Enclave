// Copyright Epic Games, Inc. All Rights Reserved.

#include "Enclave.h"
#include "Debug/EnclaveDebug.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <processthreadsapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
// Windows 11 power throttling stops honoring a background process's 1 ms timer
// request, so the engine's frame-limit Sleep() rounds up to 15.6 ms ticks: a 60 Hz
// dedicated server measured 34 Hz with four test clients running. Opt out where tick
// cadence matters: dedicated servers always, and every process in builds with debug
// code (the SSR test harness runs several background clients).
void KeepPreciseTimerResolution()
{
#ifndef PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION
#define PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION 0x4
#endif
	PROCESS_POWER_THROTTLING_STATE State = {};
	State.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
	State.ControlMask = PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION;
	State.StateMask = 0; // Never ignore this process's timer resolution request.
	if (!::SetProcessInformation(::GetCurrentProcess(), ProcessPowerThrottling, &State, sizeof(State)))
	{
		UE_LOG(LogTemp, Warning, TEXT("Enclave: could not opt out of timer-resolution power throttling (error %u)."), ::GetLastError());
	}
}
#endif

class FEnclaveGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
#if PLATFORM_WINDOWS
		if (IsRunningDedicatedServer() || ENCLAVE_INCLUDE_DEBUG_CODE)
		{
			KeepPreciseTimerResolution();
		}
#endif
	}
};
}

IMPLEMENT_PRIMARY_GAME_MODULE( FEnclaveGameModule, Enclave, "Enclave" );
