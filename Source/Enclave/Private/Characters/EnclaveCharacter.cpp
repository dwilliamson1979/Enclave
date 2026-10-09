// Copyright Lightcarrier Studios

#include "Characters/EnclaveCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Effects/EnclaveGameplayEffectScopePolicy.h"
#include "Abilities/Interfaces/EnclaveAbilitySystemConsumerInterface.h"
#include "Player/EnclavePlayerState.h"

#include "Input/Components/EnclaveInputRouterComponent.h"
#include "Input/Core/EnclaveInputConfig.h"

#include "Characters/Components/EnclaveCharacterMovementComponent.h"
#include "Characters/Components/EnclaveViewComponent.h"
#include "Characters/Components/EnclaveAvatarPresentationComponent.h"
#include "Health/Components/EnclaveGASHealthComponent.h"
#include "Lifecycle/Components/EnclaveAvatarLifecycleComponent.h"

#include "Inventory/Components/EnclaveInventoryComponent.h"

#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Game/EnclaveGameModeBase.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Debug/EnclaveDebug.h"

// Input tag tracing. Previously printed unconditionally on every input
// event; now opt-in like the other Enclave.Debug.* toggles.
static TAutoConsoleVariable<bool> CVarDebugInput(
	TEXT("Enclave.Debug.Input"),
	false,
	TEXT("Log and print every input tag the character receives in ProcessInput."),
	ECVF_Cheat);

AEnclaveCharacter::AEnclaveCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEnclaveCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set this character to call Tick() every frame. You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Camera boom
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>("SpringArmComp");
	SpringArmComp->SetupAttachment(GetRootComponent());
	SpringArmComp->TargetArmLength = 0.0f;
	SpringArmComp->bEnableCameraLag = true;
	SpringArmComp->CameraLagSpeed = 15.f;
	SpringArmComp->bUsePawnControlRotation = true;

	// What if in the future we want the player to be able to alternate between first and third person views?

	// Follow camera
	CameraComp = CreateDefaultSubobject<UCameraComponent>("CameraComp");
	CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
	CameraComp->bUsePawnControlRotation = false;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>("FirstPersonMesh");
	FirstPersonMesh->SetupAttachment(CameraComp);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetOwnerNoSee(false);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->SetReceivesDecals(false);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	FirstPersonMesh->PrimaryComponentTick.TickGroup = TG_PrePhysics;

	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetReceivesDecals(false);

	InputRouterComp = CreateDefaultSubobject<UEnclaveInputRouterComponent>(TEXT("InputRouterComp"));
	ViewComp = CreateDefaultSubobject<UEnclaveViewComponent>(TEXT("ViewComp"));

	InventoryComp = CreateDefaultSubobject<UEnclaveInventoryComponent>(TEXT("InventoryComp"));
	InventoryComp->SetIsReplicated(true);

	EquipmentComp = CreateDefaultSubobject<UEnclaveEquipmentComponent>(TEXT("EquipmentComp"));
	EquipmentComp->SetIsReplicated(true);

	AvatarPresentationComp = CreateDefaultSubobject<UEnclaveAvatarPresentationComponent>(TEXT("AvatarPresentationComp"));
	AvatarPresentationComp->SetPresentationMeshes(FirstPersonMesh, GetMesh());

	HealthComp = CreateDefaultSubobject<UEnclaveGASHealthComponent>(TEXT("HealthComp"));
	LifecycleComp = CreateDefaultSubobject<UEnclaveAvatarLifecycleComponent>(TEXT("LifecycleComp"));
}

void AEnclaveCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Server-side: PlayerState is already valid by this point.
	InitializeAbilityActorInfo();

	if (!InventoryComp || !EquipmentComp || !InventoryComp->IsEmpty())
	{
		return;
	}

	for (const TSubclassOf<AEnclaveItem>& ItemClass : DefaultItemClasses)
	{
		InventoryComp->SpawnItem(ItemClass);
	}

	for (const TObjectPtr<AEnclaveItem>& Item : InventoryComp->GetItems())
	{
		if (EquipmentComp->WieldItem(Item))
		{
			break;
		}
	}
}

void AEnclaveCharacter::UnPossessed()
{
	// The ASC is PlayerState-owned and may outlive this Pawn. Only tear down
	// avatar-local consumers here; do not clear the ASC's global actor info,
	// because a replacement avatar may initialize against the same ASC.
	UninitializeAbilitySystemConsumers();

	Super::UnPossessed();
}

void AEnclaveCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// Client-side: this is the owning client's independent notification that
	// PlayerState (and therefore the ASC) is now valid. PossessedBy above
	// only runs on the server - without this override, abilities would work
	// for the host/listen-server player and silently fail for every client.
	InitializeAbilityActorInfo();
}

void AEnclaveCharacter::InitializeAbilityActorInfo()
{
	AEnclavePlayerState* EnclavePlayerState = GetPlayerState<AEnclavePlayerState>();
	UAbilitySystemComponent* ASC = IsValid(EnclavePlayerState) ? EnclavePlayerState->GetAbilitySystemComponent() : nullptr;

	if (!IsValid(ASC))
	{
		UninitializeAbilitySystemConsumers();
		return;
	}

	// The ASC survives Pawn replacement. Remove state that belonged to its old
	// avatar before rebinding ActorInfo to this Pawn, so removal callbacks/cues
	// cannot accidentally resolve against the replacement avatar.
	InitializeAvatarEffectScopeIfNeeded(ASC);

	// Owner = PlayerState (persistent for the match), Avatar = this Pawn
	// (expected to change on respawn). Safe to call more than once - GAS
	// treats a repeat call with the same actors as a no-op.
	ASC->InitAbilityActorInfo(EnclavePlayerState, this);

	if (CachedAbilitySystemComponent.Get() == ASC)
	{
		return;
	}

	// A Character should normally see only one ASC during its lifetime, but
	// explicitly unwind an old binding before accepting a different one.
	UninitializeAbilitySystemConsumers();
	CachedAbilitySystemComponent = ASC;
	InitializeAbilitySystemConsumers(ASC);
}

void AEnclaveCharacter::InitializeAvatarEffectScopeIfNeeded(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!HasAuthority() || !IsValid(AbilitySystemComponent) || AvatarEffectScopeInitializedAbilitySystem.Get() == AbilitySystemComponent)
	{
		return;
	}

	AvatarEffectScopeInitializedAbilitySystem = AbilitySystemComponent;

	// The PlayerState ASC outlives its Pawn. Only effects explicitly classified
	// Effect.Scope.Player may survive into this new avatar. Avatar-scoped and
	// unclassified active effects are removed before ActorInfo/consumers bind.
	FEnclaveGameplayEffectScopePolicy::RemoveAvatarLifetimeEffects(
		AbilitySystemComponent,
		EEnclaveAvatarEffectCleanupReason::AvatarReplacement);
}

void AEnclaveCharacter::InitializeAbilitySystemConsumers(UAbilitySystemComponent* AbilitySystemComponent)
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	const TArray<UActorComponent*> Consumers = GetComponentsByInterface(UEnclaveAbilitySystemConsumerInterface::StaticClass());

	for (UActorComponent* Component : Consumers)
	{
		if (IEnclaveAbilitySystemConsumerInterface* Consumer = Cast<IEnclaveAbilitySystemConsumerInterface>(Component))
		{
			Consumer->InitializeWithAbilitySystem(AbilitySystemComponent);
		}
	}
}

void AEnclaveCharacter::UninitializeAbilitySystemConsumers()
{
	const TArray<UActorComponent*> Consumers = GetComponentsByInterface(UEnclaveAbilitySystemConsumerInterface::StaticClass());

	for (UActorComponent* Component : Consumers)
	{
		if (IEnclaveAbilitySystemConsumerInterface* Consumer = Cast<IEnclaveAbilitySystemConsumerInterface>(Component))
		{
			Consumer->UninitializeFromAbilitySystem();
		}
	}

	CachedAbilitySystemComponent.Reset();
}

UAbilitySystemComponent* AEnclaveCharacter::GetAbilitySystemComponent() const
{
	if (UAbilitySystemComponent* CachedASC = CachedAbilitySystemComponent.Get())
	{
		return CachedASC;
	}

	const AEnclavePlayerState* EnclavePlayerState = GetPlayerState<AEnclavePlayerState>();
	return IsValid(EnclavePlayerState) ? EnclavePlayerState->GetAbilitySystemComponent() : nullptr;
}

void AEnclaveCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyWeaponHitDetectionCollisionPolicy();

	if (InputRouterComp)
	{
		InputRouterComp->RegisterInputHandler(Cast<UEnclaveCharacterMovementComponent>(GetCharacterMovement()));
		InputRouterComp->RegisterInputHandler(ViewComp);
		InputRouterComp->RegisterInputHandler(EquipmentComp);
	}
}

void AEnclaveCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Covers client teardown and destruction paths where UnPossessed may not be
	// observed locally. Consumer uninitialization is intentionally idempotent.
	UninitializeAbilitySystemConsumers();

	Super::EndPlay(EndPlayReason);
}

void AEnclaveCharacter::ApplyWeaponHitDetectionCollisionPolicy()
{
	// Fire hit detection is server-authoritative (UEnclaveGA_Fire only traces
	// when HasAuthority), so clients do not get to choose their own target
	// representation. Collision-response changes do not need to replicate.
	if (!HasAuthority())
	{
		return;
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	USkeletalMeshComponent* ThirdPersonMesh = GetMesh();

	if (!IsValid(Capsule) || !IsValid(ThirdPersonMesh))
	{
		return;
	}

	const AEnclaveGameModeBase* EnclaveGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AEnclaveGameModeBase>() : nullptr;

	if (!IsValid(EnclaveGameMode))
	{
		ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s could not resolve AEnclaveGameModeBase; defaulting Weapon collision policy to Capsule.", *GetName());
	}

	const EEnclaveWeaponHitDetectionMode Mode = IsValid(EnclaveGameMode)
		? EnclaveGameMode->GetWeaponHitDetectionMode()
		: EEnclaveWeaponHitDetectionMode::Capsule;

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
		ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		if (!ThirdPersonMesh->GetPhysicsAsset())
		{
			ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s is using SkeletalHitboxes but its third-person mesh has no Physics Asset; Weapon traces cannot hit skeletal bodies.", *GetName());
		}

		if (ThirdPersonMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s is using SkeletalHitboxes but its third-person mesh collision is disabled; configure the CharacterMesh collision profile for query collision.", *GetName());
		}
		break;

	case EEnclaveWeaponHitDetectionMode::Capsule:
	default:
		Capsule->SetCollisionResponseToChannel(ECC_Weapon, ECR_Block);
		ThirdPersonMesh->SetCollisionResponseToChannel(ECC_Weapon, ECR_Ignore);
		break;
	}
}

// Called every frame
void AEnclaveCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AEnclaveCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnclaveCharacter::ProcessInput(const FEnclaveInputEvent& InputEvent)
{
	if (CVarDebugInput.GetValueOnGameThread())
	{
		ENCLAVE_LOG(LogEnclaveInput, Log, "%s ProcessInput: %s", *GetName(), *InputEvent.InputTag.ToString());
		ENCLAVE_SCREEN(FString::Printf(TEXT("ProcessInput: %s"), *InputEvent.InputTag.ToString()), FColor::Green, 2.0f);
	}

	if (InputRouterComp)
	{
		InputRouterComp->RouteInput(InputEvent);
	}
}

bool AEnclaveCharacter::CanProcessAvatarInput(const FEnclaveInputEvent& InputEvent) const
{
	if (!InputEvent.IsValid())
	{
		return false;
	}

	// Lifecycle is an avatar-policy implementation detail. Routers and gameplay
	// systems ask only whether this semantic input may currently be processed.
	// A future observer pawn can expose an entirely different policy/config
	// without adding death-specific exceptions here.
	return !IsValid(LifecycleComp) || !LifecycleComp->IsDead();
}

const UEnclaveInputConfig* AEnclaveCharacter::GetInputConfig() const
{
	return InputConfig;
}

FEnclaveAnimData AEnclaveCharacter::GetAnimData() const
{
	FEnclaveAnimData AnimData;

	AnimData.LocalVelocity = GetActorRotation().UnrotateVector(GetVelocity());

	// bIsCrouched is replicated by ACharacter, so this is correct on
	// simulated proxies too.
	AnimData.bIsCrouching = bIsCrouched;

	if (const UCharacterMovementComponent* MovementComp = GetCharacterMovement())
	{
		AnimData.bIsFalling = MovementComp->IsFalling();

		// KNOWN GAP: current acceleration is only meaningful on the server
		// and the owning client - the engine doesn't replicate it to
		// simulated proxies, so this reads false for other players. Nothing
		// in the vertical slice depends on it (locomotion keys off speed).
		// When start/stop transitions need it, replicate a compressed
		// acceleration here (Lyra's FLyraReplicatedAcceleration pattern);
		// anim instances won't change.
		AnimData.bIsAccelerating = !MovementComp->GetCurrentAcceleration().IsNearlyZero();
	}

	// Locally/server: controller rotation. Simulated proxies: rebuilt from
	// ACharacter's replicated RemoteViewPitch, which arrives as 0..360 -
	// normalize to -180..180 (effectively -90..90 for pitch).
	AnimData.AimPitch = FRotator::NormalizeAxis(GetBaseAimRotation().Pitch);

	return AnimData;
}
