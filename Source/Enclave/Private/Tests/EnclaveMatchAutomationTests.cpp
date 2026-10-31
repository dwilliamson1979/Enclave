// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for match rules and settings.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Game/EnclaveMatchSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveMatchSettingsOptionsTest,
	"Enclave.Match.Settings.ApplyOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveMatchSettingsOptionsTest::RunTest(const FString& Parameters)
{
	const FEnclaveMatchSettings Defaults;

	FEnclaveMatchSettings Settings;
	Settings.ApplyOptions(TEXT(""));
	TestEqual(TEXT("No options keep the score limit"), Settings.ScoreLimit, Defaults.ScoreLimit);
	TestEqual(TEXT("No options keep min players"), Settings.MinPlayersToStart, Defaults.MinPlayersToStart);

	Settings.ApplyOptions(TEXT("?ScoreLimit=3?TimeLimit=0?RespawnDelay=1?Warmup=0?MinPlayers=4?PostMatch=5"));
	TestEqual(TEXT("ScoreLimit applied"), Settings.ScoreLimit, 3);
	TestEqual(TEXT("TimeLimit 0 (no limit) applied"), Settings.TimeLimitSeconds, 0);
	TestEqual(TEXT("RespawnDelay applied"), Settings.RespawnDelaySeconds, 1);
	TestEqual(TEXT("Warmup applied"), Settings.WarmupSeconds, 0);
	TestEqual(TEXT("MinPlayers applied"), Settings.MinPlayersToStart, 4);
	TestEqual(TEXT("PostMatch applied"), Settings.PostMatchSeconds, 5);

	// Invalid values log a warning and keep the current value.
	AddExpectedMessage(TEXT("Ignoring invalid"), EAutomationExpectedMessageFlags::Contains, 5);
	Settings.ApplyOptions(TEXT("?ScoreLimit=abc?TimeLimit=-5?MinPlayers=0?Warmup=2.5?RespawnDelay=3abc"));
	TestEqual(TEXT("Non-integer score limit ignored"), Settings.ScoreLimit, 3);
	TestEqual(TEXT("Trailing junk respawn delay ignored"), Settings.RespawnDelaySeconds, 1);
	TestEqual(TEXT("Negative time limit ignored"), Settings.TimeLimitSeconds, 0);
	TestEqual(TEXT("Zero min players ignored"), Settings.MinPlayersToStart, 4);
	TestEqual(TEXT("Fractional warmup ignored"), Settings.WarmupSeconds, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
