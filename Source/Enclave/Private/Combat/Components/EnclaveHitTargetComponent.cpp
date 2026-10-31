// Copyright Lightcarrier Studios

#include "Combat/Components/EnclaveHitTargetComponent.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include "Combat/Components/EnclaveCombatPolicyComponent.h"
#include "Combat/Components/EnclaveTargetHistoryComponent.h"
#include "Combat/EnclaveSSRTimingPolicy.h"

bool FEnclaveSSRServerCapsuleObservation::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	Ar.SerializeIntPacked(ServerTimeMs);
	bOutSuccess = !Ar.IsError();
	return true;
}

UEnclaveHitTargetComponent::UEnclaveHitTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEnclaveHitTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!HasOwnerAuthority())
	{
		return;
	}

	TargetHistory = GetOwner()->FindComponentByClass<UEnclaveTargetHistoryComponent>();
	if (IsValid(TargetHistory))
	{
		TargetHistory->OnCapsuleFrameRecorded().AddUObject(this, &UEnclaveHitTargetComponent::HandleCapsuleFrameRecorded);
	}

	UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(GetWorld());
	EEnclaveWeaponHitDetectionMode HitDetectionMode = IsValid(CombatPolicy) ? CombatPolicy->GetWeaponHitDetectionMode() : EEnclaveWeaponHitDetectionMode::Capsule;

	if (!IsValid(CombatPolicy))
		ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s found no combat policy on the game mode; defaulting Weapon collision policy to Capsule.", *GetNameSafe(GetOwner()));
	else
		CombatPolicy->OnWeaponHitDetectionModeChanged().AddUObject(this, &UEnclaveHitTargetComponent::HandleWeaponHitDetectionModeChanged);

	ApplyWeaponHitDetectionCollisionPolicy(HitDetectionMode);
}

void UEnclaveHitTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(TargetHistory))
	{
		TargetHistory->OnCapsuleFrameRecorded().RemoveAll(this);
	}

	if (HasOwnerAuthority())
	{
		if (UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(GetWorld()))
		{
			CombatPolicy->OnWeaponHitDetectionModeChanged().RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool UEnclaveHitTargetComponent::HasOwnerAuthority() const
{
	const AActor* Owner = GetOwner();
	return Owner && Owner->HasAuthority();
}

void UEnclaveHitTargetComponent::HandleWeaponHitDetectionModeChanged(EEnclaveWeaponHitDetectionMode NewMode)
{
	ApplyWeaponHitDetectionCollisionPolicy(NewMode);
}

void UEnclaveHitTargetComponent::ApplyWeaponHitDetectionCollisionPolicy(EEnclaveWeaponHitDetectionMode Mode)
{
	if (!HasOwnerAuthority()) { return; }
	if (IsValid(TargetHistory))
	{
		TargetHistory->InvalidateHistory(); // Never span a collision-policy transition.
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	USkeletalMeshComponent* ThirdPersonMesh = Character ? Character->GetMesh() : nullptr;

	if (!IsValid(Capsule) || !IsValid(ThirdPersonMesh)) { return; }

	switch (Mode)
	{
	case EEnclaveWeaponHitDetectionMode::SkeletalHitboxes:
		// Movement still uses the capsule. Only Weapon queries ignore it and
		// continue to the third-person mesh's Physics Asset bodies.
		Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
		ThirdPersonMesh->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);

		// A dedicated server has no rendering to force bone refreshes. Weapon
		// traces depend on current Physics Asset body transforms, so skeletal
		// hitbox mode explicitly keeps the authoritative body pose current.
		if (!bSavedThirdPersonAnimTickOption)
		{
			SavedThirdPersonAnimTickOption = ThirdPersonMesh->VisibilityBasedAnimTickOption;
			bSavedThirdPersonAnimTickOption = true;
		}
		ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		if (!ThirdPersonMesh->GetPhysicsAsset())
		{
			ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s is using SkeletalHitboxes but its third-person mesh has no Physics Asset; Weapon traces cannot hit skeletal bodies.", *GetNameSafe(GetOwner()));
		}

		if (ThirdPersonMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s is using SkeletalHitboxes but its third-person mesh collision is disabled; configure the CharacterMesh collision profile for query collision.", *GetNameSafe(GetOwner()));
		}
		break;

	case EEnclaveWeaponHitDetectionMode::Capsule:
	default:
		Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
		ThirdPersonMesh->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
		if (bSavedThirdPersonAnimTickOption)
		{
			ThirdPersonMesh->VisibilityBasedAnimTickOption = SavedThirdPersonAnimTickOption;
			bSavedThirdPersonAnimTickOption = false;
		}
		break;
	}
}

void UEnclaveHitTargetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UEnclaveHitTargetComponent, SSRServerCapsuleObservation, COND_SimulatedOnly);
}

void UEnclaveHitTargetComponent::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
#if ENCLAVE_INCLUDE_DEBUG_CODE
	// Shadow diagnostics read stamps in every mode; the cvar exists for bandwidth A/B runs.
	static const IConsoleVariable* ReplicateStamps =
		IConsoleManager::Get().FindConsoleVariable(TEXT("Enclave.Debug.SSRReplicateStamps"));
	const bool bReplicateSSRStamps = !ReplicateStamps || ReplicateStamps->GetBool();
#else
	const UEnclaveCombatPolicyComponent* CombatPolicy = UEnclaveCombatPolicyComponent::Get(GetWorld());
	const bool bReplicateSSRStamps = CombatPolicy && CombatPolicy->RequiresClientSSRStamps();
#endif
	DOREPLIFETIME_ACTIVE_OVERRIDE_FAST(UEnclaveHitTargetComponent, SSRServerCapsuleObservation, bReplicateSSRStamps);
}

void UEnclaveHitTargetComponent::HandleCapsuleFrameRecorded(const FEnclaveTargetCapsuleFrame& Frame)
{
	if (!HasOwnerAuthority() || !FMath::IsFinite(Frame.ServerTimeSeconds))
	{
		return;
	}
	// Only publish a new stamp when the capsule changed: an unchanged pose needs no
	// new timing evidence, and an unchanged property lets idle characters go quiet.
	constexpr double MinMoveCm = 0.5;
	const bool bChanged = SSRServerCapsuleObservation.ServerTimeMs == 0 || Frame.bBreakBefore ||
		FVector::DistSquared(Frame.Center, LastStampedCenter) > MinMoveCm * MinMoveCm ||
		!FMath::IsNearlyEqual(Frame.RadiusCm, LastStampedRadiusCm, 0.5f) ||
		!FMath::IsNearlyEqual(Frame.HalfHeightCm, LastStampedHalfHeightCm, 0.5f);
	if (!bChanged)
	{
		return;
	}
	SSRServerCapsuleObservation.ServerTimeMs = EnclaveSSRTimingPolicy::QuantizeServerTimeMs(Frame.ServerTimeSeconds);
	LastStampedCenter = Frame.Center;
	LastStampedRadiusCm = Frame.RadiusCm;
	LastStampedHalfHeightCm = Frame.HalfHeightCm;
}

void UEnclaveHitTargetComponent::OnRep_SSRServerCapsuleObservation()
{
	const AActor* Owner = GetOwner();
	if (!Owner || GetOwnerRole() != ROLE_SimulatedProxy || !GetWorld())
	{
		return;
	}
	FEnclaveSSRReceivedProxyUpdate& Update = LastReceivedSSRObservation;
	Update.ServerStamp = EnclaveSSRTimingPolicy::DequantizeServerTimeMs(SSRServerCapsuleObservation.ServerTimeMs);
	// The stamp carries no geometry; diagnostics compare against the proxy at receipt.
	Update.ReplicatedLocation = Owner->GetActorLocation();
	Update.ReplicatedVelocity = FVector::ZeroVector;
	Update.CapsuleAtReceive = Owner->GetActorLocation();
	Update.ClientReceiveWorldSeconds = GetWorld()->GetTimeSeconds();
	++Update.ReceiveCount;
#if ENCLAVE_INCLUDE_DEBUG_CODE
	DebugReceivedSSRStamps.Add(Update.ServerStamp);
	while (DebugReceivedSSRStamps.Num() > 0 && DebugReceivedSSRStamps[0] < Update.ServerStamp - 1.0)
	{
		DebugReceivedSSRStamps.RemoveAt(0);
	}
#endif
}
