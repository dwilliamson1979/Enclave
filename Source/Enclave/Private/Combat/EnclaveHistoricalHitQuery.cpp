// Copyright Lightcarrier Studios

#include "Combat/EnclaveHistoricalHitQuery.h"

#include "Characters/EnclaveCharacter.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"
#include "Combat/EnclaveHistoricalCapsuleIntersection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

FEnclaveHistoricalHitQuery::FEnclaveHistoricalHitQuery(UWorld* InWorld, const FVector& InStart,
	const FVector& InEnd, AActor* InShooter)
	: World(InWorld), Shooter(InShooter), Start(InStart), End(InEnd)
{
	if (!IsValid(InWorld) || InWorld->GetNetMode() == NM_Client ||
		!IsValid(InShooter) || !InShooter->HasAuthority() || InShooter->GetWorld() != InWorld ||
		Start.ContainsNaN() || End.ContainsNaN() || Start.Equals(End) ||
		!FMath::IsFinite(FVector::Dist(Start, End)))
	{
		return;
	}

	// Mirror Stage C3's world-only obstruction semantics: current character
	// bodies cannot be used to occlude recorded historical target positions.
	FCollisionQueryParams WorldParams(SCENE_QUERY_STAT(EnclaveHistoricalHitWorld), true);
	WorldParams.AddIgnoredActor(InShooter);
	TArray<AActor*> AttachedActors;
	InShooter->GetAttachedActors(AttachedActors);
	WorldParams.AddIgnoredActors(AttachedActors);
	for (TActorIterator<AEnclaveCharacter> It(InWorld); It; ++It)
	{
		if (IsValid(*It))
		{
			WorldParams.AddIgnoredActor(*It);
		}
	}

	bWorldBlocked = InWorld->LineTraceSingleByChannel(WorldHit, Start, End, ECC_Weapon, WorldParams);
	WorldBlockDistanceCm = bWorldBlocked ? FVector::Dist(Start, WorldHit.ImpactPoint) : -1.0;
	bPrepared = true;
}

bool FEnclaveHistoricalHitQuery::IsPrepared() const
{
	return bPrepared && World.IsValid() && Shooter.IsValid() &&
		Shooter->HasAuthority() && Shooter->GetWorld() == World.Get();
}

FEnclaveHistoricalHitQueryResult FEnclaveHistoricalHitQuery::Trace(double HistoryTimeSeconds, const AEnclaveCharacter* OnlyTarget) const
{
	FEnclaveHistoricalHitQueryResult Result;
	if (!IsPrepared() || !FMath::IsFinite(HistoryTimeSeconds) || HistoryTimeSeconds < 0.0)
	{
		return Result;
	}

	UWorld* ServerWorld = World.Get();
	AActor* Instigator = Shooter.Get();
	double NearestDistanceCm = FVector::Dist(Start, End) + 1.0;
	for (TActorIterator<AEnclaveCharacter> It(ServerWorld); It; ++It)
	{
		AEnclaveCharacter* Target = *It;
		if (!IsValid(Target) || Target == Instigator || (OnlyTarget && Target != OnlyTarget))
		{
			continue;
		}

		const UEnclaveTargetHistoryComponent* History = Target->FindComponentByClass<UEnclaveTargetHistoryComponent>();
		FEnclaveTargetCapsuleFrame Capsule;
		if (!IsValid(History) || !History->TryGetCapsuleAtTime(HistoryTimeSeconds, Capsule))
		{
			++Result.Missing;
			continue;
		}

		++Result.Available;
		double EntryDistanceCm = 0.0;
		FVector HitPoint;
		if (FEnclaveHistoricalCapsuleIntersection::TryIntersectSegment(Start, End, Capsule,
			EntryDistanceCm, HitPoint) && EntryDistanceCm < NearestDistanceCm)
		{
			NearestDistanceCm = EntryDistanceCm;
			Result.Target = Target;
			Result.EntryDistanceCm = EntryDistanceCm;
			Result.Point = HitPoint;
		}
	}

	Result.bOccluded = Result.Target.IsValid() && bWorldBlocked && WorldBlockDistanceCm < Result.EntryDistanceCm;
	Result.bHit = Result.Target.IsValid() && !Result.bOccluded;
	return Result;
}
