// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"

class AActor;
class AEnclaveCharacter;
class UWorld;

/**
 * Read-only capsule-rewind result. A candidate may be present but occluded by
 * present-time blocking geometry; bHit is true only for an unobstructed hit.
 * Available/Missing currently count ALL other Enclave characters (not just
 * those close to the ray). They are diagnostics, not a match-wide fallback rule.
 */
struct ENCLAVE_API FEnclaveHistoricalHitQueryResult
{
	TWeakObjectPtr<AEnclaveCharacter> Target;
	FVector Point = FVector::ZeroVector;
	double EntryDistanceCm = -1.0;
	int32 Available = 0;
	int32 Missing = 0;
	bool bOccluded = false;
	bool bHit = false;
};

/**
 * Shared, always-compiled historical capsule query for SSR diagnostics and
 * eventual authoritative hit registration. Construction performs exactly one
 * PRESENT-TIME blocking-world trace (ignoring character bodies); Trace() then
 * evaluates historical character capsules without moving live actors.
 *
 * No damage, ammo, timing selection, rewind authorization, or fallback policy
 * is implemented here. The caller must validate timing, representation mode,
 * and gameplay policy independently before using a returned hit.
 *
 * A query is short-lived, for one shot, on the authoritative game thread.
 * Present-time world obstruction intentionally stays fixed across multiple
 * historical-time comparisons for that shot. It is NOT historical cover rewind.
 */
class ENCLAVE_API FEnclaveHistoricalHitQuery
{
public:
	FEnclaveHistoricalHitQuery(UWorld* InWorld, const FVector& InStart, const FVector& InEnd, AActor* InShooter);

	bool IsPrepared() const;
	const FHitResult& GetWorldHit() const { return WorldHit; }
	bool IsWorldBlocked() const { return bWorldBlocked; }
	double GetWorldBlockDistanceCm() const { return WorldBlockDistanceCm; }

	/** Returns a miss/empty result on invalid time or unavailable query context. */
	FEnclaveHistoricalHitQueryResult Trace(double HistoryTimeSeconds) const;

private:
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AActor> Shooter;
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FHitResult WorldHit;
	double WorldBlockDistanceCm = -1.0;
	bool bWorldBlocked = false;
	bool bPrepared = false;
};
