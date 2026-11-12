// Copyright Lightcarrier Studios

#include "Combat/Components/EnclaveDamageHistoryComponent.h"

#include "Combat/EnclaveDamageTaken.h"
#include "Debug/EnclaveDebug.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Health/Components/EnclaveHealthComponent.h"

UEnclaveDamageHistoryComponent::UEnclaveDamageHistoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

void UEnclaveDamageHistoryComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || !Owner->HasAuthority())
	{
		return;
	}

	UEnclaveHealthComponent* HealthComponent = UEnclaveHealthComponent::FindHealthComponent(Owner);
	if (!IsValid(HealthComponent))
	{
		UE_LOG(LogEnclaveCombat, Warning, TEXT("%s has a damage history but no health component; nothing will be recorded."), *GetNameSafe(Owner));
		return;
	}

	HealthComponent->OnDamageTaken.AddUniqueDynamic(this, &ThisClass::HandleDamageTaken);
	BoundHealthComponent = HealthComponent;
}

void UEnclaveDamageHistoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEnclaveHealthComponent* HealthComponent = BoundHealthComponent.Get())
	{
		HealthComponent->OnDamageTaken.RemoveDynamic(this, &ThisClass::HandleDamageTaken);
	}
	BoundHealthComponent.Reset();
	Entries.Reset();

	Super::EndPlay(EndPlayReason);
}

void UEnclaveDamageHistoryComponent::HandleDamageTaken(UEnclaveHealthComponent* HealthComponent, const FEnclaveDamageTaken& DamageTaken)
{
	RecordDamage(DamageTaken);
}

void UEnclaveDamageHistoryComponent::RecordDamage(const FEnclaveDamageTaken& DamageTaken)
{
	if (DamageTaken.HealthDamage <= 0.0f)
	{
		return;
	}

	const double Now = GetServerTime();
	Prune(Now);

	// Make room for the new entry, oldest first.
	const int32 Overflow = Entries.Num() - FMath::Max(MaxEntries, 1) + 1;
	if (Overflow > 0)
	{
		Entries.RemoveAt(0, Overflow, EAllowShrinking::No);
	}

	FEnclaveDamageHistoryEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Attacker = DamageTaken.Request.SourcePlayerState;
	Entry.DamageCauser = DamageTaken.Request.DamageCauser;
	Entry.HealthDamage = DamageTaken.HealthDamage;
	Entry.ServerTime = Now;
	Entry.DamageTags = DamageTaken.Request.DamageTags;
}

TConstArrayView<FEnclaveDamageHistoryEntry> UEnclaveDamageHistoryComponent::GetEntriesSince(double ServerTime) const
{
	// Time order, so the matching entries are a tail of the array.
	int32 First = Entries.Num();
	while (First > 0 && Entries[First - 1].ServerTime >= ServerTime)
	{
		--First;
	}
	return TConstArrayView<FEnclaveDamageHistoryEntry>(Entries).RightChop(First);
}

double UEnclaveDamageHistoryComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}

	// On the server this is the world time; using the server clock keeps entries
	// comparable with every other timestamp the match uses.
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void UEnclaveDamageHistoryComponent::Prune(double Now)
{
	const double Oldest = Now - MaxEntryAgeSeconds;
	int32 Expired = 0;
	while (Expired < Entries.Num() && Entries[Expired].ServerTime < Oldest)
	{
		++Expired;
	}
	if (Expired > 0)
	{
		Entries.RemoveAt(0, Expired, EAllowShrinking::No);
	}
}

FString UEnclaveDamageHistoryComponent::DescribeEntries() const
{
	const double Now = GetServerTime();
	FString Result;
	for (const FEnclaveDamageHistoryEntry& Entry : Entries)
	{
		const APlayerState* Attacker = Entry.Attacker.Get();
		Result += FString::Printf(TEXT("\n  %.2f s ago: %.1f from %s (causer %s, tags %s)"),
			Now - Entry.ServerTime,
			Entry.HealthDamage,
			Attacker ? *Attacker->GetPlayerName() : TEXT("no player"),
			*GetNameSafe(Entry.DamageCauser.Get()),
			*Entry.DamageTags.ToStringSimple());
	}
	return Result.IsEmpty() ? TEXT(" (empty)") : Result;
}
