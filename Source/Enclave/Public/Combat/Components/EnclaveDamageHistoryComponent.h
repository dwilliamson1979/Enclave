// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EnclaveDamageHistoryComponent.generated.h"

class AActor;
class APlayerState;
class UEnclaveHealthComponent;
struct FEnclaveDamageTaken;

/** One hit that took something from this life. Server time domain. */
struct ENCLAVE_API FEnclaveDamageHistoryEntry
{
	/** The player responsible (the request's SourcePlayerState); null for the environment. */
	TWeakObjectPtr<APlayerState> Attacker;

	/** What physically dealt the damage (weapon, projectile), if anything. */
	TWeakObjectPtr<AActor> DamageCauser;

	/** Health actually lost. */
	float HealthDamage = 0.0f;

	/** When the damage landed, from GameState->GetServerWorldTimeSeconds(). */
	double ServerTime = 0.0;

	FGameplayTagContainer DamageTags;
};

/**
 * Server-only, unreplicated record of the damage one life took, for kill
 * credit and assists. A new pawn per life gives a fresh history.
 *
 * Records every hit that took health (hostile, friendly, self, environment);
 * queries filter by attitude. Bounded by age and count, not by attacker:
 * entries older than MaxEntryAgeSeconds are dropped, then the oldest past
 * MaxEntries. Entries are kept in time order, oldest first.
 */
UCLASS(ClassGroup = (Enclave), meta = (BlueprintSpawnableComponent))
class ENCLAVE_API UEnclaveDamageHistoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnclaveDamageHistoryComponent();

	/** Adds one hit. Server only. Knows nothing about where the damage came from. */
	void RecordDamage(const FEnclaveDamageTaken& DamageTaken);

	/** Every entry still held, oldest first. */
	TConstArrayView<FEnclaveDamageHistoryEntry> GetEntries() const { return Entries; }

	/** Entries that landed at or after ServerTime, oldest first. */
	TConstArrayView<FEnclaveDamageHistoryEntry> GetEntriesSince(double ServerTime) const;

	/**
	 * The attacker of the most recent entry at or after ServerTime that Accept
	 * takes, or null. Entries with no attacker, or one who has left, are skipped.
	 * The caller decides who counts (e.g. hostile to the victim).
	 */
	APlayerState* FindMostRecentAttacker(double ServerTime, TFunctionRef<bool(const APlayerState&)> Accept) const;

	/** One line per entry, for logs. */
	FString DescribeEntries() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Entries older than this, in seconds, are dropped. Longer than any credit or assist window. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage History", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MaxEntryAgeSeconds = 30.0f;

	/** Hard cap; the oldest entries are dropped first. */
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Damage History", meta = (ClampMin = "1"))
	int32 MaxEntries = 64;

private:
	UFUNCTION()
	void HandleDamageTaken(UEnclaveHealthComponent* HealthComponent, const FEnclaveDamageTaken& DamageTaken);

	double GetServerTime() const;
	void Prune(double Now);

	TArray<FEnclaveDamageHistoryEntry> Entries;

	TWeakObjectPtr<UEnclaveHealthComponent> BoundHealthComponent;
};
