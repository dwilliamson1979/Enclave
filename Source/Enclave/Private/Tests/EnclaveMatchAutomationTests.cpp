// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for match rules and settings.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Game/EnclaveMatchSettings.h"
#include "Game/EnclaveTeamAssignment.h"
#include "Math/RandomStream.h"

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

namespace EnclaveTeamAssignmentTests
{
	constexpr uint8 NoTeam = AEnclaveTeamPlayerState::NoTeam;

	/** Solo groups with these previous teams; JoinOrder is the index (later = newer). */
	TArray<FEnclaveTeamGroup> SoloGroups(std::initializer_list<uint8> PreviousTeams)
	{
		TArray<FEnclaveTeamGroup> Groups;
		for (const uint8 Team : PreviousTeams)
		{
			FEnclaveTeamGroup& Group = Groups.AddDefaulted_GetRef();
			Group.PreviousTeam = Team;
			Group.JoinOrder = Groups.Num() - 1;
		}
		return Groups;
	}

	/** Players per team for a result. */
	TArray<int32> TeamSizes(const TArray<FEnclaveTeamGroup>& Groups, const TArray<uint8>& Teams, int32 NumTeams)
	{
		TArray<int32> Sizes;
		Sizes.Init(0, NumTeams);
		for (int32 Index = 0; Index < Groups.Num(); ++Index)
		{
			if (Teams[Index] < NumTeams)
			{
				Sizes[Teams[Index]] += Groups[Index].Size;
			}
		}
		return Sizes;
	}

	FString Describe(const TArray<int32>& Values)
	{
		return FString::JoinBy(Values, TEXT(","), [](int32 Value) { return FString::FromInt(Value); });
	}

	FString Describe(const TArray<uint8>& Values)
	{
		return FString::JoinBy(Values, TEXT(","), [](uint8 Value) { return FString::FromInt(Value); });
	}

	FEnclaveTeamAssignmentRules Rules(bool bKeepTeams)
	{
		FEnclaveTeamAssignmentRules Result;
		Result.bKeepTeams = bKeepTeams;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveTeamAssignmentKeepTest,
	"Enclave.Match.TeamAssignment.Keep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveTeamAssignmentKeepTest::RunTest(const FString& Parameters)
{
	using namespace EnclaveTeamAssignmentTests;
	FRandomStream Random(1);

	// Balanced teams stay as they are.
	{
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 1, 0, 1 });
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("Balanced: unchanged"), Describe(Teams), FString(TEXT("0,1,0,1")));
	}

	// 4v2 evens out to 3v3 with one move: the newest player on the big team.
	{
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 0, 1, 0, 1, 0 });
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("4v2: the newest on team 0 moved"), Describe(Teams), FString(TEXT("0,0,1,0,1,1")));
	}

	// No team, or a team this mode doesn't have, goes to the short team.
	{
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 0, 1, NoTeam });
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("NoTeam: placed on the short team"), Describe(Teams), FString(TEXT("0,0,1,1")));
	}
	{
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 0, 1, 7 });
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("Out of range: placed on the short team"), Describe(Teams), FString(TEXT("0,0,1,1")));
	}

	// A larger MaxTeamSizeDifference tolerates 4v2.
	{
		FEnclaveTeamAssignmentRules Loose = Rules(true);
		Loose.MaxTeamSizeDifference = 2;
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 0, 0, 0, 1, 1 });
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Loose, 2, Random);
		TestEqual(TEXT("Tolerance 2: unchanged"), Describe(Teams), FString(TEXT("0,0,0,0,1,1")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveTeamAssignmentRandomTest,
	"Enclave.Match.TeamAssignment.Random",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveTeamAssignmentRandomTest::RunTest(const FString& Parameters)
{
	using namespace EnclaveTeamAssignmentTests;

	// KeepTeams off: everyone is placed anew, sizes within 1, and the seed decides.
	{
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 0, 0, 0, 0, 1, 1, 1, 1 });
		FRandomStream RandomA(1234);
		FRandomStream RandomB(1234);
		const TArray<uint8> TeamsA = EnclaveTeamAssignment::AssignTeams(Groups, Rules(false), 2, RandomA);
		const TArray<uint8> TeamsB = EnclaveTeamAssignment::AssignTeams(Groups, Rules(false), 2, RandomB);
		const TArray<int32> Sizes = TeamSizes(Groups, TeamsA, 2);
		TestTrue(*FString::Printf(TEXT("Random: sizes within 1 (%s)"), *Describe(Sizes)), FMath::Abs(Sizes[0] - Sizes[1]) <= 1);
		TestEqual(TEXT("Random: same seed, same teams"), Describe(TeamsA), Describe(TeamsB));
	}

	// Three teams, seven new players: 3/2/2.
	{
		const TArray<FEnclaveTeamGroup> Groups = SoloGroups({ NoTeam, NoTeam, NoTeam, NoTeam, NoTeam, NoTeam, NoTeam });
		FRandomStream Random(7);
		TArray<int32> Sizes = TeamSizes(Groups, EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 3, Random), 3);
		Sizes.Sort();
		TestEqual(TEXT("Three teams: 2/2/3"), Describe(Sizes), FString(TEXT("2,2,3")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveTeamAssignmentGroupsTest,
	"Enclave.Match.TeamAssignment.Groups",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveTeamAssignmentGroupsTest::RunTest(const FString& Parameters)
{
	using namespace EnclaveTeamAssignmentTests;

	// A group of 3 and three solos, all new: the group is placed first, so 3v3.
	{
		TArray<FEnclaveTeamGroup> Groups = SoloGroups({ NoTeam, NoTeam, NoTeam, NoTeam });
		Groups[3].Size = 3;
		FRandomStream Random(3);
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("Group first: 3v3"), Describe(TeamSizes(Groups, Teams, 2)), FString(TEXT("3,3")));
		TestTrue(TEXT("Group first: the solos are together"), Teams[0] == Teams[1] && Teams[1] == Teams[2] && Teams[2] != Teams[3]);
	}

	// Evening out moves solos, not the group, when either would do.
	{
		TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 0, 0, 0, 0, 1 });
		Groups[4].Size = 2;   // team 0: 4 solos + a group of 2 = 6; team 1: 1
		FRandomStream Random(5);
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("Solos move first: 4v3"), Describe(TeamSizes(Groups, Teams, 2)), FString(TEXT("4,3")));
		TestEqual(TEXT("Solos move first: the group stays"), static_cast<int32>(Teams[4]), 0);
	}

	// Best effort: a group of 3 and one solo can't get within 1, and nothing loops.
	{
		TArray<FEnclaveTeamGroup> Groups = SoloGroups({ 0, 1 });
		Groups[0].Size = 3;
		FRandomStream Random(9);
		const TArray<uint8> Teams = EnclaveTeamAssignment::AssignTeams(Groups, Rules(true), 2, Random);
		TestEqual(TEXT("Best effort: unchanged 3v1"), Describe(Teams), FString(TEXT("0,1")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveTeamAssignmentOptionsTest,
	"Enclave.Match.TeamAssignment.ApplyOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveTeamAssignmentOptionsTest::RunTest(const FString& Parameters)
{
	FEnclaveTeamAssignmentRules Rules;
	Rules.ApplyOptions(TEXT("?TeamAssignment=random?KeepTeams=0"));
	TestTrue(TEXT("Short, lower-case policy name"), Rules.Assignment == EEnclaveTeamAssignment::Random);
	TestFalse(TEXT("KeepTeams=0"), Rules.bKeepTeams);

	Rules.ApplyOptions(TEXT("?KeepTeams=true"));
	TestTrue(TEXT("KeepTeams=true"), Rules.bKeepTeams);

	// Invalid values log a warning and keep the current value.
	AddExpectedMessage(TEXT("Ignoring invalid"), EAutomationExpectedMessageFlags::Contains, 3);
	Rules.ApplyOptions(TEXT("?TeamAssignment=SkillBalance?KeepTeams=maybe"));
	Rules.ApplyOptions(TEXT("?TeamAssignment=EEnclaveTeamAssignment_MAX"));
	TestTrue(TEXT("Unknown policy ignored"), Rules.Assignment == EEnclaveTeamAssignment::Random);
	TestTrue(TEXT("Bad KeepTeams ignored"), Rules.bKeepTeams);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
