// Copyright Lightcarrier Studios
// Deterministic, no-PIE automation coverage for match rules and settings.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Game/EnclaveMatchSettings.h"
#include "Game/EnclaveSpawnSelection.h"
#include "Game/EnclaveTeamAssignment.h"
#include "Game/EnclaveTeamRoster.h"
#include "Game/EnclaveURLOptions.h"
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

	Settings.ApplyOptions(TEXT("?SuicideScore=-1"));
	TestEqual(TEXT("Negative SuicideScore applied"), Settings.SuicideScore, -1);

	Settings.ApplyOptions(TEXT("?AssistScore=1"));
	TestEqual(TEXT("AssistScore applied"), Settings.AssistScore, 1);

	// Invalid values log a warning and keep the current value.
	AddExpectedMessage(TEXT("Ignoring invalid"), EAutomationExpectedMessageFlags::Contains, 6);
	Settings.ApplyOptions(TEXT("?ScoreLimit=abc?TimeLimit=-5?MinPlayers=0?Warmup=2.5?RespawnDelay=3abc?AssistScore=-1"));
	TestEqual(TEXT("Non-integer score limit ignored"), Settings.ScoreLimit, 3);
	TestEqual(TEXT("Trailing junk respawn delay ignored"), Settings.RespawnDelaySeconds, 1);
	TestEqual(TEXT("Negative time limit ignored"), Settings.TimeLimitSeconds, 0);
	TestEqual(TEXT("Zero min players ignored"), Settings.MinPlayersToStart, 4);
	TestEqual(TEXT("Fractional warmup ignored"), Settings.WarmupSeconds, 0);
	TestEqual(TEXT("Negative AssistScore ignored"), Settings.AssistScore, 1);
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

	TestTrue(TEXT("Team slots are Open by default"), Rules.TeamSlots == EEnclaveTeamSlots::Open);
	Rules.ApplyOptions(TEXT("?TeamSlots=reserved?ReservedSlotHold=120"));
	TestTrue(TEXT("TeamSlots=reserved"), Rules.TeamSlots == EEnclaveTeamSlots::Reserved);
	TestEqual(TEXT("ReservedSlotHold=120"), Rules.ReservedSlotHoldSeconds, 120.0f);
	Rules.ApplyOptions(TEXT("?ReservedSlotHold=0"));
	TestEqual(TEXT("ReservedSlotHold=0 (the whole match)"), Rules.ReservedSlotHoldSeconds, 0.0f);

	// Invalid values log a warning and keep the current value.
	AddExpectedMessage(TEXT("Ignoring invalid"), EAutomationExpectedMessageFlags::Contains, 5);
	Rules.ApplyOptions(TEXT("?TeamAssignment=SkillBalance?KeepTeams=maybe"));
	Rules.ApplyOptions(TEXT("?TeamAssignment=EEnclaveTeamAssignment_MAX"));
	Rules.ApplyOptions(TEXT("?TeamSlots=Closed?ReservedSlotHold=-5"));
	TestTrue(TEXT("Unknown policy ignored"), Rules.Assignment == EEnclaveTeamAssignment::Random);
	TestTrue(TEXT("Bad KeepTeams ignored"), Rules.bKeepTeams);
	TestTrue(TEXT("Unknown team slots ignored"), Rules.TeamSlots == EEnclaveTeamSlots::Reserved);
	TestEqual(TEXT("Negative hold ignored"), Rules.ReservedSlotHoldSeconds, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveTeamRosterTest,
	"Enclave.Match.TeamRoster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveTeamRosterTest::RunTest(const FString& Parameters)
{
	// Hold for the whole match (0).
	FEnclaveTeamRoster Roster;
	Roster.Add(TEXT("A"), 0);
	Roster.Add(TEXT("B"), 0);
	Roster.Add(TEXT("C"), 1);
	TestTrue(TEXT("A connected player holds their team"), Roster.FindHeldTeam(TEXT("A"), 10.0, 0.0) == TOptional<uint8>(0));
	TestFalse(TEXT("A stranger holds nothing"), Roster.FindHeldTeam(TEXT("X"), 10.0, 0.0).IsSet());
	TestEqual(TEXT("No one absent"), Roster.CountAllAbsentHeld(10.0, 0.0), 0);

	Roster.MarkLeft(TEXT("A"), 10.0);
	Roster.MarkLeft(TEXT("X"), 10.0);   // not on the roster: nothing happens
	TestEqual(TEXT("A's place on team 0 is held"), Roster.CountAbsentHeld(0, 1000.0, 0.0), 1);
	TestEqual(TEXT("Nothing held on team 1"), Roster.CountAbsentHeld(1, 1000.0, 0.0), 0);
	TestEqual(TEXT("One place held in all"), Roster.CountAllAbsentHeld(1000.0, 0.0), 1);
	TestTrue(TEXT("A still gets team 0 much later"), Roster.FindHeldTeam(TEXT("A"), 1000.0, 0.0) == TOptional<uint8>(0));

	// A returning player is connected again: no longer an absent place.
	Roster.Add(TEXT("A"), 0);
	TestEqual(TEXT("A is back"), Roster.CountAllAbsentHeld(1000.0, 0.0), 0);

	// Hold for 60 s.
	Roster.MarkLeft(TEXT("C"), 100.0);
	TestTrue(TEXT("Held before the time is up"), Roster.FindHeldTeam(TEXT("C"), 159.0, 60.0) == TOptional<uint8>(1));
	TestEqual(TEXT("Counted before the time is up"), Roster.CountAbsentHeld(1, 159.0, 60.0), 1);
	TestFalse(TEXT("Expired at the hold time"), Roster.FindHeldTeam(TEXT("C"), 160.0, 60.0).IsSet());
	TestEqual(TEXT("Not counted once expired"), Roster.CountAbsentHeld(1, 160.0, 60.0), 0);

	// Removing frees the place (a player who came back as a spectator).
	Roster.MarkLeft(TEXT("B"), 200.0);
	Roster.Remove(TEXT("B"));
	TestEqual(TEXT("B's place is freed"), Roster.CountAbsentHeld(0, 200.0, 0.0), 0);

	Roster.Reset();
	TestFalse(TEXT("Reset empties the roster"), Roster.FindHeldTeam(TEXT("A"), 200.0, 0.0).IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveURLOptionsTest,
	"Enclave.Match.URLOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveURLOptionsTest::RunTest(const FString& Parameters)
{
	// Floats: strict, in range.
	float Scale = 1.0f;
	EnclaveURLOptions::ApplyFloat(TEXT("?Scale=0.5"), TEXT("Scale"), 0.0f, 1.0f, Scale);
	TestEqual(TEXT("Float applied"), Scale, 0.5f);
	EnclaveURLOptions::ApplyFloat(TEXT("?Other=0.25"), TEXT("Scale"), 0.0f, 1.0f, Scale);
	TestEqual(TEXT("Missing float key keeps the value"), Scale, 0.5f);

	AddExpectedMessage(TEXT("Ignoring invalid"), EAutomationExpectedMessageFlags::Contains, 4);
	EnclaveURLOptions::ApplyFloat(TEXT("?Scale=1.5"), TEXT("Scale"), 0.0f, 1.0f, Scale);
	EnclaveURLOptions::ApplyFloat(TEXT("?Scale=0.5.5"), TEXT("Scale"), 0.0f, 1.0f, Scale);
	EnclaveURLOptions::ApplyFloat(TEXT("?Scale=half"), TEXT("Scale"), 0.0f, 1.0f, Scale);
	EnclaveURLOptions::ApplyFloat(TEXT("?Scale=-0.1"), TEXT("Scale"), 0.0f, 1.0f, Scale);
	TestEqual(TEXT("Bad floats keep the value"), Scale, 0.5f);

	// Bools: 1/0/true/false.
	bool bFlag = false;
	EnclaveURLOptions::ApplyBool(TEXT("?Flag=1"), TEXT("Flag"), bFlag);
	TestTrue(TEXT("Bool 1"), bFlag);
	EnclaveURLOptions::ApplyBool(TEXT("?Flag=FALSE"), TEXT("Flag"), bFlag);
	TestFalse(TEXT("Bool FALSE"), bFlag);

	// Strict numbers.
	int32 IntValue = 0;
	float FloatValue = 0.0f;
	TestTrue(TEXT("Int 42"), EnclaveURLOptions::TryParseStrictInt(TEXT("42"), IntValue) && IntValue == 42);
	TestFalse(TEXT("Int 2.5"), EnclaveURLOptions::TryParseStrictInt(TEXT("2.5"), IntValue));
	TestTrue(TEXT("Float .5"), EnclaveURLOptions::TryParseStrictFloat(TEXT(".5"), FloatValue) && FloatValue == 0.5f);
	TestFalse(TEXT("Float ."), EnclaveURLOptions::TryParseStrictFloat(TEXT("."), FloatValue));
	TestFalse(TEXT("Float 1e3"), EnclaveURLOptions::TryParseStrictFloat(TEXT("1e3"), FloatValue));
	return true;
}


namespace EnclaveSpawnSelectionTests
{
	/** Starts along the X axis, 1000 cm apart: 0, 1000, 2000, ... */
	TArray<FEnclaveSpawnCandidate> StartsAlongX(int32 Count)
	{
		TArray<FEnclaveSpawnCandidate> Candidates;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Candidates.Add({ FVector(1000.0 * Index, 0.0, 0.0), false });
		}
		return Candidates;
	}

	/** Which candidates RunCount picks with a fresh seeded stream each run. */
	TSet<int32> PicksOverSeeds(TConstArrayView<FEnclaveSpawnCandidate> Candidates, TConstArrayView<FVector> Enemies,
		const FEnclaveSpawnRules& Rules, int32 RunCount = 200)
	{
		TSet<int32> Picks;
		for (int32 Seed = 0; Seed < RunCount; ++Seed)
		{
			FRandomStream Random(Seed);
			Picks.Add(EnclaveSpawnSelection::ChooseStart(Candidates, Enemies, Rules, Random));
		}
		return Picks;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnclaveSpawnSelectionTest,
	"Enclave.Match.SpawnSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnclaveSpawnSelectionTest::RunTest(const FString& Parameters)
{
	using namespace EnclaveSpawnSelectionTests;

	FEnclaveSpawnRules Rules;
	FRandomStream Random(1);

	// No candidates.
	TestEqual(TEXT("No candidates: none"), EnclaveSpawnSelection::ChooseStart({}, {}, Rules, Random), INDEX_NONE);

	// Farthest start alone in the margin: with an enemy at 0, starts score 0,
	// 1000, 2000, 3000, 4000; 80% of 4000 is 3200, so only start 4.
	{
		const TArray<FEnclaveSpawnCandidate> Candidates = StartsAlongX(5);
		const TArray<FVector> Enemies = { FVector::ZeroVector };
		const TSet<int32> Picks = PicksOverSeeds(Candidates, Enemies, Rules);
		TestTrue(TEXT("Farthest wins when alone in the margin"), Picks.Num() == 1 && Picks.Contains(4));
	}

	// The nearest enemy decides: enemies at both ends make the middle best.
	{
		const TArray<FEnclaveSpawnCandidate> Candidates = StartsAlongX(5);
		const TArray<FVector> Enemies = { FVector(0.0, 0.0, 0.0), FVector(4000.0, 0.0, 0.0) };
		const TSet<int32> Picks = PicksOverSeeds(Candidates, Enemies, Rules);
		TestTrue(TEXT("Nearest enemy decides: the middle"), Picks.Num() == 1 && Picks.Contains(2));
	}

	// Random within the margin: at 0.5, starts 2..4 (2000+ of 4000) qualify.
	{
		FEnclaveSpawnRules WideRules;
		WideRules.RandomMarginFraction = 0.5f;
		const TArray<FEnclaveSpawnCandidate> Candidates = StartsAlongX(5);
		const TArray<FVector> Enemies = { FVector::ZeroVector };
		const TSet<int32> Picks = PicksOverSeeds(Candidates, Enemies, WideRules);
		TestTrue(TEXT("Margin 0.5: picks 2, 3 and 4 only"), Picks.Num() == 3 && Picks.Contains(2) && Picks.Contains(3) && Picks.Contains(4));

		FRandomStream RandomA(42);
		FRandomStream RandomB(42);
		TestEqual(TEXT("Same seed, same pick"),
			EnclaveSpawnSelection::ChooseStart(Candidates, Enemies, WideRules, RandomA),
			EnclaveSpawnSelection::ChooseStart(Candidates, Enemies, WideRules, RandomB));
	}

	// No enemies: any start.
	{
		const TArray<FEnclaveSpawnCandidate> Candidates = StartsAlongX(4);
		const TSet<int32> Picks = PicksOverSeeds(Candidates, {}, Rules);
		TestEqual(TEXT("No enemies: every start can be picked"), Picks.Num(), 4);
	}

	// Blocked starts are skipped, even when they score best.
	{
		TArray<FEnclaveSpawnCandidate> Candidates = StartsAlongX(5);
		Candidates[4].bBlocked = true;
		const TArray<FVector> Enemies = { FVector::ZeroVector };
		const TSet<int32> Picks = PicksOverSeeds(Candidates, Enemies, Rules);
		TestTrue(TEXT("Blocked best skipped: the next farthest"), Picks.Num() == 1 && Picks.Contains(3));
	}

	// All blocked: scored among the blocked ones as a last resort.
	{
		TArray<FEnclaveSpawnCandidate> Candidates = StartsAlongX(3);
		for (FEnclaveSpawnCandidate& Candidate : Candidates)
		{
			Candidate.bBlocked = true;
		}
		const TArray<FVector> Enemies = { FVector::ZeroVector };
		const TSet<int32> Picks = PicksOverSeeds(Candidates, Enemies, Rules);
		TestTrue(TEXT("All blocked: the farthest blocked start"), Picks.Num() == 1 && Picks.Contains(2));
	}

	// Margin 1: always a best start; ties stay random.
	{
		FEnclaveSpawnRules StrictRules;
		StrictRules.RandomMarginFraction = 1.0f;
		const TArray<FEnclaveSpawnCandidate> Candidates = {
			{ FVector(-2000.0, 0.0, 0.0), false }, { FVector(500.0, 0.0, 0.0), false }, { FVector(2000.0, 0.0, 0.0), false } };
		const TArray<FVector> Enemies = { FVector::ZeroVector };
		const TSet<int32> Picks = PicksOverSeeds(Candidates, Enemies, StrictRules);
		TestTrue(TEXT("Margin 1: the two tied best starts"), Picks.Num() == 2 && Picks.Contains(0) && Picks.Contains(2));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
