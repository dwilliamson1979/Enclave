// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Input/Interfaces/EnclaveInputReceiverInterface.h"
#include "Input/Interfaces/EnclaveInputConfigProviderInterface.h"
#include "Input/Interfaces/EnclaveAvatarInputPolicyInterface.h"
#include "Animation/Interfaces/EnclaveAnimDataProviderInterface.h"
#include "Components/SkinnedMeshComponent.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Debug/EnclaveDebug.h"
#include "GameplayTagContainer.h"
#include "EnclaveCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UEnclaveInputConfig;
class UEnclaveInputRouterComponent;
class UEnclaveCharacterMovementComponent;
class UEnclaveViewComponent;
class UEnclaveEquipmentComponent;
class UEnclaveInventoryComponent;
class AEnclaveItem;
class UEnclaveAvatarPresentationComponent;
class UAbilitySystemComponent;
class UEnclaveGASHealthComponent;
class UEnclaveAvatarLifecycleComponent;
class UEnclaveTargetHistoryComponent;

/** Author-defined starting inventory entry with an explicit equipment slot. */
USTRUCT(BlueprintType)
struct FEnclaveStartingSlottedItem
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory")
	TSubclassOf<AEnclaveItem> ItemClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment", meta = (Categories = "Equipment.Slot"))
	FGameplayTag SlotTag;
};

/** Server-authored time and capsule geometry from a single authoritative history frame.
 * Replication supplies a server-clock anchor; it does not replace FRepMovement. */
USTRUCT()
struct FEnclaveSSRServerCapsuleObservation
{
	GENERATED_BODY()

	UPROPERTY() double ServerTimeSeconds = 0.0;
	UPROPERTY() FVector_NetQuantize10 Center = FVector::ZeroVector;
	UPROPERTY() float RadiusCm = 0.0f;
	UPROPERTY() float HalfHeightCm = 0.0f;
};

/** Last received authoritative observation; local arrival time is not a network clock. */
struct FEnclaveSSRReceivedProxyUpdate
{
	double ServerStamp = 0.0;
	FVector ReplicatedLocation = FVector::ZeroVector;
	FVector ReplicatedVelocity = FVector::ZeroVector;
	FVector CapsuleAtReceive = FVector::ZeroVector;
	double ClientReceiveWorldSeconds = -1.0;
	uint32 ReceiveCount = 0;
};

UCLASS()
class ENCLAVE_API AEnclaveCharacter : public ACharacter, public IEnclaveInputReceiverInterface, public IEnclaveInputConfigProviderInterface, public IEnclaveAvatarInputPolicyInterface, public IAbilitySystemInterface, public IEnclaveAnimDataProviderInterface
{
	GENERATED_BODY()

public:
	AEnclaveCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaTime) override;
	virtual void OnRep_ReplicatedMovement() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	/** Called when the history component publishes an authoritative sample. */
	void HandleSSRHistoryFrameRecorded(const struct FEnclaveTargetCapsuleFrame& Frame);
	const FEnclaveSSRReceivedProxyUpdate& GetLastReceivedSSRObservation() const { return LastReceivedSSRObservation; }

#if ENCLAVE_INCLUDE_DEBUG_CODE
	const FEnclaveSSRReceivedProxyUpdate& DebugGetLastReceivedProxyUpdate() const { return DebugLastReceivedProxyUpdate; }



	// Server-only, transient D1.1B test protection. This is never replicated and
	// is enabled only while the local-authority debug strafe rig is active.
	void DebugSetSSRStrafeDamageSuppressed(bool bSuppress) { if (HasAuthority()) { bDebugSSRStrafeDamageSuppressed = bSuppress; } }
	bool DebugIsSSRStrafeDamageSuppressed() const { return HasAuthority() && bDebugSSRStrafeDamageSuppressed; }
#endif
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	virtual bool TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest = false, bool bNoCheck = false) override;

	/**
	 * Canonical first-person gameplay origin. APawn::GetActorEyesViewPoint
	 * obtains its location here, so server hit registration uses the same
	 * camera component that the owning player aims through.
	 */
	virtual FVector GetPawnViewLocation() const override;

	/** Development-only breakdown of the live first-person view chain for shot diagnostics. */
	void DebugLogViewGeometryForShot(int32 PredictionKey, const FVector& RenderedViewLocation) const;

	/**
	 * Client-side counterpart to PossessedBy: fires when the replicated
	 * PlayerState reference lands on a client, which happens on a different
	 * timeline than server-side possession. Both paths must independently
	 * (re)point the ASC's avatar at this Pawn - this is the single most
	 * common GAS setup bug (abilities silently not working on one side of
	 * the connection because actor info was only initialized on the other).
	 */
	virtual void OnRep_PlayerState() override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// Interface implementations

	// IEnclaveInputReceiverInterface
	virtual void ProcessInput(const FEnclaveInputEvent& InputEvent) override;

	// IEnclaveInputConfigProviderInterface
	virtual const UEnclaveInputConfig* GetInputConfig() const override;

	// IEnclaveAvatarInputPolicyInterface
	virtual bool CanProcessAvatarInput(const FEnclaveInputEvent& InputEvent) const override;

	// IEnclaveAnimDataProviderInterface
	virtual FEnclaveAnimData GetAnimData() const override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// A single replicated struct carries one coherent authoritative history sample.
	UPROPERTY(ReplicatedUsing=OnRep_SSRServerCapsuleObservation, Transient)
	FEnclaveSSRServerCapsuleObservation SSRServerCapsuleObservation;

	UFUNCTION()
	void OnRep_SSRServerCapsuleObservation();

	FEnclaveSSRReceivedProxyUpdate LastReceivedSSRObservation;

#if ENCLAVE_INCLUDE_DEBUG_CODE
	// Updated by OnRep_ReplicatedMovement on simulated proxies only.
	FEnclaveSSRReceivedProxyUpdate DebugLastReceivedProxyUpdate;
	bool bDebugSSRStrafeDamageSuppressed = false;
#endif
	/** Shared by PossessedBy (server) and OnRep_PlayerState (client) - both must call this independently. */
	void InitializeAbilityActorInfo();
	/** Server-owned lifetime for an optional external/intrinsic item; Equipment never spawns it. */
	void InitializePersistentItem();
	void InitializeStowedItem();

	/** Initializes all avatar components that opt into the ASC-consumer contract. */
	void InitializeAbilitySystemConsumers(UAbilitySystemComponent* AbilitySystemComponent);

	/** Removes ASC bindings from all avatar components currently consuming it. */
	void UninitializeAbilitySystemConsumers();

	/** Applies persistent-ASC effect lifetime policy exactly once before this Pawn binds as avatar. */
	void InitializeAvatarEffectScopeIfNeeded(UAbilitySystemComponent* AbilitySystemComponent);

	/** Responds to authoritative match hit-detection policy changes. */
	void HandleOnWeaponHitDetectionModeChanged(EEnclaveWeaponHitDetectionMode NewMode);
	/** Applies the selected policy to this character's collision components. Authority only. */
	void ApplyWeaponHitDetectionCollisionPolicy(EEnclaveWeaponHitDetectionMode Mode);

	// Preserve the configured animation tick behavior when temporarily forcing skeletal hitboxes.
	EVisibilityBasedAnimTickOption SavedThirdPersonAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	bool bSavedThirdPersonAnimTickOption = false;

	// BlueprintReadOnly so BP_EnclaveCharacter's graph can reach it (e.g. to
	// test Link Anim Class Layers before the equip-driven C++ path exists).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USpringArmComponent> SpringArmComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraComponent> CameraComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "View", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveViewComponent> ViewComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveInputRouterComponent> InputRouterComp = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveInputConfig> InputConfig = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveInventoryComponent> InventoryComp = nullptr;

	/** Initial carried items with no Equipment slot assignment (e.g. supplies).
	 * Use StartingSlottedItems for equipment that starts assigned. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Inventory|Starting Loadout", meta = (AllowPrivateAccess = "true"))
	TArray<TSubclassOf<AEnclaveItem>> StartingInventoryItems;

	/** Explicit initial item-to-slot assignments; only these enter Equipment's cycle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Starting Loadout", meta = (AllowPrivateAccess = "true"))
	TArray<FEnclaveStartingSlottedItem> StartingSlottedItems;

	/** Actor-lifetime Default item; name preserved so existing Blueprint assets retain their class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Intrinsic Items", meta = (AllowPrivateAccess = "true", DisplayName = "Default Item Class"))
	TSubclassOf<AEnclaveItem> PersistentItemClass;

	/** Optional actor-lifetime Stowed item. Null disables the feature. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment|Intrinsic Items", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AEnclaveItem> StowedItemClass;

	/** Server-side lifetime owner. Not an inventory entry or an Equipment-owned item. */
	UPROPERTY(Transient)
	TObjectPtr<AEnclaveItem> PersistentItem = nullptr;

	/** Server-side lifetime owner for the optional, per-archetype Stowed item. */
	UPROPERTY(Transient)
	TObjectPtr<AEnclaveItem> StowedItem = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveEquipmentComponent> EquipmentComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Avatar Presentation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveAvatarPresentationComponent> AvatarPresentationComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveGASHealthComponent> HealthComp = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lifecycle", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveAvatarLifecycleComponent> LifecycleComp = nullptr;

	/** Nonreplicated, server-only Stage C2 collision capsule history. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Lag Compensation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UEnclaveTargetHistoryComponent> TargetHistoryComp = nullptr;

	/** Non-owning cache of the PlayerState-owned ASC currently initialized for this avatar. */
	TWeakObjectPtr<UAbilitySystemComponent> CachedAbilitySystemComponent;

	/**
	 * Remembers which persistent ASC has already had its previous avatar-lifetime state
	 * swept for this Pawn. It deliberately survives UnPossessed so re-binding
	 * this same Pawn to the same ASC does not clear valid effects or revive it.
	 */
	TWeakObjectPtr<UAbilitySystemComponent> AvatarEffectScopeInitializedAbilitySystem;

};
