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
#include "Combat/Components/EnclaveTargetHistoryComponent.h"

#include "Inventory/Components/EnclaveInventoryComponent.h"

#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayTags/EnclaveEquipmentSlotTags.h"
#include "Collision/EnclaveCollisionChannels.h"
#include "Game/EnclaveGameModeBase.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
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
	// The first-person gameplay eye is the camera component. Its position
	// must not depend on a time-integrated spring-arm lag or a collision probe.
	SpringArmComp->bEnableCameraLag = false;
	SpringArmComp->bEnableCameraRotationLag = false;
	SpringArmComp->bDoCollisionTest = false;
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
	ViewComp->SetViewCamera(CameraComp);
	ViewComp->SetFirstPersonMesh(FirstPersonMesh);

	InventoryComp = CreateDefaultSubobject<UEnclaveInventoryComponent>(TEXT("InventoryComp"));
	InventoryComp->SetIsReplicated(true);

	EquipmentComp = CreateDefaultSubobject<UEnclaveEquipmentComponent>(TEXT("EquipmentComp"));
	EquipmentComp->SetIsReplicated(true);

	AvatarPresentationComp = CreateDefaultSubobject<UEnclaveAvatarPresentationComponent>(TEXT("AvatarPresentationComp"));
	AvatarPresentationComp->SetPresentationMeshes(FirstPersonMesh, GetMesh());

	HealthComp = CreateDefaultSubobject<UEnclaveGASHealthComponent>(TEXT("HealthComp"));
	LifecycleComp = CreateDefaultSubobject<UEnclaveAvatarLifecycleComponent>(TEXT("LifecycleComp"));
	TargetHistoryComp = CreateDefaultSubobject<UEnclaveTargetHistoryComponent>(TEXT("TargetHistoryComp"));
}

FVector AEnclaveCharacter::GetPawnViewLocation() const
{
	// Keep the authored camera offset (including Blueprint relative transforms)
	// rather than forcing the visual camera to the Character's BaseEyeHeight.
	// This component also exists on authority for remote-controlled Pawns.
	if (IsValid(CameraComp) && CameraComp->IsRegistered())
	{
		return CameraComp->GetComponentLocation();
	}
	return Super::GetPawnViewLocation();
}

void AEnclaveCharacter::DebugLogViewGeometryForShot(int32 PredictionKey, const FVector& RenderedViewLocation) const
{
#if ENCLAVE_INCLUDE_DEBUG_CODE
	if (!IsValid(CameraComp) || !IsValid(SpringArmComp))
	{
		return;
	}

	const FVector Pivot = SpringArmComp->GetComponentLocation();
	const FVector Socket = SpringArmComp->GetSocketLocation(USpringArmComponent::SocketName);
	const FVector Camera = CameraComp->GetComponentLocation();
	const FVector Eye = GetPawnViewLocation();
	ENCLAVE_LOG(LogEnclaveCombat, Log,
		"ShotAlign GEOMETRY key=%d renderedToCamera=%.2fcm cameraToPivot=%.2fcm cameraToSocket=%.2fcm cameraToGameplayEye=%.2fcm arm=%.2fcm lag=%d rotLag=%d collision=%d",
		PredictionKey, FVector::Dist(RenderedViewLocation, Camera), FVector::Dist(Camera, Pivot),
		FVector::Dist(Camera, Socket), FVector::Dist(Camera, Eye), SpringArmComp->TargetArmLength,
		SpringArmComp->bEnableCameraLag ? 1 : 0, SpringArmComp->bEnableCameraRotationLag ? 1 : 0,
		SpringArmComp->bDoCollisionTest ? 1 : 0);
#else
	(void)PredictionKey;
	(void)RenderedViewLocation;
#endif
}

void AEnclaveCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (HasAuthority() && IsValid(TargetHistoryComp))
	{
		TargetHistoryComp->InvalidateHistory(); // New controller/avatar ownership boundary.
	}

	// Server-side: PlayerState is already valid by this point.
	InitializeAbilityActorInfo();
	if (!IsValid(EquipmentComp))
	{
		return;
	}

	// Explicitly provide one source; Equipment does not discover components.
	EquipmentComp->SetItemSource(InventoryComp);
	InitializePersistentItem();
	InitializeStowedItem();

	// Create regular source-owned items and assign only explicitly named slots.
	// Unassigned ordinary inventory items never participate in Next/Previous.
	if (IsValid(InventoryComp) && InventoryComp->IsEmpty())
	{
		for (const FEnclaveStartingSlottedItem& Entry : StartingSlottedItems)
		{
			if (!Entry.ItemClass || !Entry.SlotTag.IsValid())
			{
				continue;
			}
			if (AEnclaveItem* Item = InventoryComp->SpawnItem(Entry.ItemClass))
			{
				if (!EquipmentComp->AssignSourceItemToSlot(Entry.SlotTag, Item))
				{
					UE_LOG(LogEnclaveCharacter, Warning, TEXT("%s: could not assign %s to slot %s; item remains in Inventory."),
						*GetName(), *GetNameSafe(Item), *Entry.SlotTag.ToString());
				}
			}
		}

		// Inventory-only items are distinct from equipment assignments. They
		// enter Inventory but are deliberately not selected or placed in a slot.
		for (const TSubclassOf<AEnclaveItem>& ItemClass : StartingInventoryItems)
		{
			if (ItemClass)
			{
				InventoryComp->SpawnItem(ItemClass);
			}
		}
	}

	// Starting ordinary slotted item takes precedence over fallback. The item
	// selection is explicit and separate from its lifetime/membership.
	if (!IsValid(EquipmentComp->GetCurrentWieldedItem()))
	{
		bool bSelectedStartingItem = false;
		for (const FEnclaveStartingSlottedItem& Entry : StartingSlottedItems)
		{
			if (IsValid(EquipmentComp->GetItemInSlot(Entry.SlotTag)) && EquipmentComp->SelectSlot(Entry.SlotTag))
			{
				bSelectedStartingItem = true;
				break;
			}
		}
		if (!bSelectedStartingItem)
		{
			EquipmentComp->SelectSlot(EnclaveGameplayTags::Equipment_Slot_Default);
		}
	}
}

void AEnclaveCharacter::InitializePersistentItem()
{
	if (!HasAuthority() || !IsValid(EquipmentComp) || !PersistentItemClass)
	{
		return;
	}

	if (IsValid(PersistentItem))
	{
		// Repeated possession must not create a duplicate actor.
		EquipmentComp->AssignExternalItemToSlot(EnclaveGameplayTags::Equipment_Slot_Default, PersistentItem);
		return;
	}
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnclaveItem* Item = World->SpawnActor<AEnclaveItem>(PersistentItemClass, GetActorTransform(), Params);
	if (!IsValid(Item))
	{
		return;
	}
	Item->SetActorHiddenInGame(true);
	if (!EquipmentComp->AssignExternalItemToSlot(EnclaveGameplayTags::Equipment_Slot_Default, Item))
	{
		Item->Destroy();
		UE_LOG(LogEnclaveCharacter, Warning, TEXT("%s: persistent item must satisfy the configured Equipment.Slot.Default requirements."), *GetName());
		return;
	}
	PersistentItem = Item;
}

void AEnclaveCharacter::InitializeStowedItem()
{
	if (!HasAuthority() || !IsValid(EquipmentComp))
	{
		return;
	}
	// Stowed is optional and configured only on the avatar archetype.
	const TSubclassOf<AEnclaveItem> ItemClass = StowedItemClass;
	if (!ItemClass)
	{
		return; // Optional: no Stowed assignment or dependency on normal Equipment.
	}
	if (IsValid(StowedItem))
	{
		EquipmentComp->AssignExternalItemToSlot(EnclaveGameplayTags::Equipment_Slot_Stowed, StowedItem);
		return;
	}
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AEnclaveItem* Item = World->SpawnActor<AEnclaveItem>(ItemClass, GetActorTransform(), Params);
	if (!IsValid(Item))
	{
		return;
	}
	Item->SetActorHiddenInGame(true);
	if (!EquipmentComp->AssignExternalItemToSlot(EnclaveGameplayTags::Equipment_Slot_Stowed, Item))
	{
		UE_LOG(LogEnclaveCharacter, Warning, TEXT("%s: Stowed item %s failed Equipment.Slot.Stowed assignment; check Item.Capability.Wieldable and owner."),
			*GetName(), *GetNameSafe(Item));
		Item->Destroy();
		return;
	}
	StowedItem = Item;
}

void AEnclaveCharacter::UnPossessed()
{
	if (HasAuthority() && IsValid(TargetHistoryComp))
	{
		TargetHistoryComp->InvalidateHistory();
	}
	// A PlayerState-owned ASC can outlive this pawn. Never leave the outgoing
	// pawn's camera/first-person arms with local Focus transforms applied.
	if (IsValid(ViewComp))
	{
		ViewComp->ResetFocus(/*bImmediate=*/true);
	}

	// The ASC is PlayerState-owned and may outlive this Pawn. Only tear down
	// avatar-local consumers here; do not clear the ASC's global actor info,
	// because a replacement avatar may initialize against the same ASC.
	UninitializeAbilitySystemConsumers();

	Super::UnPossessed();
}

bool AEnclaveCharacter::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck)
{
	const bool bSucceeded = Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
	if (bSucceeded && !bIsATest && HasAuthority() && IsValid(TargetHistoryComp))
	{
		TargetHistoryComp->MarkHistoryDiscontinuity(); // Keep before/after frames but never interpolate through teleport.
	}
	return bSucceeded;
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
	if (IsValid(EquipmentComp) && HasAuthority())
	{
		EquipmentComp->RefreshEquippedEffects();
	}
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

	// Blueprints can override native component defaults. Enforce the gameplay
	// contract on every world instance (owning client AND authority), including
	// a server that never renders the first-person arms.
	if (IsValid(SpringArmComp))
	{
		SpringArmComp->bEnableCameraLag = false;
		SpringArmComp->bEnableCameraRotationLag = false;
		SpringArmComp->bDoCollisionTest = false;
	}

	if (IsValid(EquipmentComp))
	{
		EquipmentComp->SetItemSource(InventoryComp);
	}

	if (HasAuthority() && IsValid(TargetHistoryComp))
	{
		TargetHistoryComp->OnCapsuleFrameRecorded().AddUObject(this, &AEnclaveCharacter::HandleSSRHistoryFrameRecorded);
	}

	if (HasAuthority())
	{
		AEnclaveGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AEnclaveGameModeBase>() : nullptr;
		EEnclaveWeaponHitDetectionMode HitDetectionMode = IsValid(GameMode) ? GameMode->GetWeaponHitDetectionMode() : EEnclaveWeaponHitDetectionMode::Capsule;

		if (!IsValid(GameMode))
			ENCLAVE_LOG(LogEnclaveCharacter, Warning, "%s could not resolve AEnclaveGameModeBase; defaulting Weapon collision policy to Capsule.", *GetName());
		else
			GameMode->OnWeaponHitDetectionModeChanged().AddUObject(this, &AEnclaveCharacter::HandleOnWeaponHitDetectionModeChanged);

		ApplyWeaponHitDetectionCollisionPolicy(HitDetectionMode);
	}

	if (InputRouterComp)
	{
		InputRouterComp->RegisterInputHandler(Cast<UEnclaveCharacterMovementComponent>(GetCharacterMovement()));
		InputRouterComp->RegisterInputHandler(ViewComp);
		InputRouterComp->RegisterInputHandler(EquipmentComp);
	}
}

void AEnclaveCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(TargetHistoryComp))
	{
		TargetHistoryComp->OnCapsuleFrameRecorded().RemoveAll(this);
	}

	if (HasAuthority())
	{
		// Revoke GAS item grants before destroying the externally owned item.
		if (IsValid(EquipmentComp))
		{
			EquipmentComp->ClearCurrentWieldedItem();
		}
		if (IsValid(PersistentItem))
		{
			PersistentItem->Destroy();
			PersistentItem = nullptr;
		}
		if (IsValid(StowedItem))
		{
			StowedItem->Destroy();
			StowedItem = nullptr;
		}
	}

	// Covers client teardown and destruction paths where UnPossessed may not be
	// observed locally. Consumer uninitialization is intentionally idempotent.
	UninitializeAbilitySystemConsumers();

	if (HasAuthority())
	{
		if (AEnclaveGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AEnclaveGameModeBase>() : nullptr)
		{
			GameMode->OnWeaponHitDetectionModeChanged().RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AEnclaveCharacter::HandleOnWeaponHitDetectionModeChanged(EEnclaveWeaponHitDetectionMode NewMode)
{
	ApplyWeaponHitDetectionCollisionPolicy(NewMode);
}

void AEnclaveCharacter::ApplyWeaponHitDetectionCollisionPolicy(EEnclaveWeaponHitDetectionMode Mode)
{
	if (!HasAuthority()) { return; }
	if (IsValid(TargetHistoryComp))
	{
		TargetHistoryComp->InvalidateHistory(); // Never span a collision-policy transition.
	}

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	USkeletalMeshComponent* ThirdPersonMesh = GetMesh();

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
		if (bSavedThirdPersonAnimTickOption)
		{
			ThirdPersonMesh->VisibilityBasedAnimTickOption = SavedThirdPersonAnimTickOption;
			bSavedThirdPersonAnimTickOption = false;
		}
		break;
	}
}

// Called every frame
void AEnclaveCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(AEnclaveCharacter, SSRServerCapsuleObservation, COND_SimulatedOnly);
}

void AEnclaveCharacter::HandleSSRHistoryFrameRecorded(const FEnclaveTargetCapsuleFrame& Frame)
{
	if (!HasAuthority() || !FMath::IsFinite(Frame.ServerTimeSeconds))
	{
		return;
	}
	SSRServerCapsuleObservation.ServerTimeSeconds = Frame.ServerTimeSeconds;
	SSRServerCapsuleObservation.Center = Frame.Center;
	SSRServerCapsuleObservation.RadiusCm = Frame.RadiusCm;
	SSRServerCapsuleObservation.HalfHeightCm = Frame.HalfHeightCm;
}

void AEnclaveCharacter::OnRep_SSRServerCapsuleObservation()
{
	if (GetLocalRole() != ROLE_SimulatedProxy || !GetWorld())
	{
		return;
	}
	FEnclaveSSRReceivedProxyUpdate& Update = LastReceivedSSRObservation;
	Update.ServerStamp = SSRServerCapsuleObservation.ServerTimeSeconds;
	Update.ReplicatedLocation = SSRServerCapsuleObservation.Center;
	Update.ReplicatedVelocity = FVector::ZeroVector;
	Update.CapsuleAtReceive = SSRServerCapsuleObservation.Center;
	Update.ClientReceiveWorldSeconds = GetWorld()->GetTimeSeconds();
	++Update.ReceiveCount;
}

void AEnclaveCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
#if ENCLAVE_INCLUDE_DEBUG_CODE
	// The world clock here is only used for LOCAL time since a movement update.
	// No client/server clock conversion and no changes to Unreal's movement simulation.
	if (GetLocalRole() != ROLE_SimulatedProxy || !GetWorld() || !GetCapsuleComponent())
	{
		return;
	}
	const float Stamp = GetReplicatedServerLastTransformUpdateTimeStamp();
	if (!FMath::IsFinite(Stamp) || Stamp <= 0.0f)
	{
		return;
	}
	const FRepMovement& Received = GetReplicatedMovement();
	DebugLastReceivedProxyUpdate.ServerStamp = Stamp;
	DebugLastReceivedProxyUpdate.ReplicatedLocation = Received.Location;
	DebugLastReceivedProxyUpdate.ReplicatedVelocity = Received.LinearVelocity;
	DebugLastReceivedProxyUpdate.CapsuleAtReceive = GetCapsuleComponent()->GetComponentLocation();
	DebugLastReceivedProxyUpdate.ClientReceiveWorldSeconds = GetWorld()->GetTimeSeconds();
	++DebugLastReceivedProxyUpdate.ReceiveCount;
#endif
}

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
