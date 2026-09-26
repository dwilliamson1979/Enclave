// Copyright Lightcarrier Studios

#include "Characters/EnclaveCharacter.h"

#include "Camera/CameraComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "Player/EnclavePlayerState.h"

#include "Input/Components/EnclaveInputRouterComponent.h"
#include "Input/Core/EnclaveInputConfig.h"

#include "Characters/Components/EnclaveCharacterMovementComponent.h"
#include "Characters/Components/EnclaveViewComponent.h"

#include "Inventory/Components/EnclaveInventoryComponent.h"

#include "Equipment/Components/EnclaveEquipmentComponent.h"
#include "Equipment/Data/EnclaveItemPresentationDefinition.h"
#include "Equipment/Items/EnclaveItem.h"

#include "Engine/Engine.h"

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

	if (!IsValid(EnclavePlayerState))
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = EnclavePlayerState->GetAbilitySystemComponent())
	{
		// Owner = PlayerState (persistent for the match), Avatar = this Pawn
		// (expected to change on respawn). Safe to call more than once -
		// GAS treats a repeat call with the same actors as a no-op.
		ASC->InitAbilityActorInfo(EnclavePlayerState, this);
	}
}

UAbilitySystemComponent* AEnclaveCharacter::GetAbilitySystemComponent() const
{
	const AEnclavePlayerState* EnclavePlayerState = GetPlayerState<AEnclavePlayerState>();
	return IsValid(EnclavePlayerState) ? EnclavePlayerState->GetAbilitySystemComponent() : nullptr;
}

void AEnclaveCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (InputRouterComp)
	{
		InputRouterComp->RegisterInputHandler(GetEnclaveCharacterMovement());
		InputRouterComp->RegisterInputHandler(ViewComp);
		InputRouterComp->RegisterInputHandler(EquipmentComp);
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
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Green,
			FString::Printf(TEXT("AEnclaveCharacter::ProcessInput: %s"), *InputEvent.InputTag.ToString()));
	}

	if (InputRouterComp)
	{
		InputRouterComp->RouteInput(InputEvent);
	}
}

const UEnclaveInputConfig* AEnclaveCharacter::GetInputConfig() const
{
	return InputConfig;
}

UEnclaveCharacterMovementComponent* AEnclaveCharacter::GetEnclaveCharacterMovement() const
{
	return Cast<UEnclaveCharacterMovementComponent>(GetCharacterMovement());
}

bool AEnclaveCharacter::ResolveWieldedItemAttachmentTargets(const FGameplayTag& ItemType, FEnclaveWieldedItemAttachmentTargets& OutTargets) const
{
	if (!ItemType.IsValid() || !IsValid(ItemPresentationDefinition))
	{
		return false;
	}

	const FEnclaveItemPresentationMapping* PresentationMapping = ItemPresentationDefinition->FindPresentationMapping(ItemType);

	if (!PresentationMapping)
	{
		return false;
	}

	if (!IsValid(FirstPersonMesh) || !IsValid(GetMesh()))
	{
		return false;
	}

	if (PresentationMapping->FirstPersonWieldedSocket.IsNone() ||
		PresentationMapping->ThirdPersonWieldedSocket.IsNone())
	{
		return false;
	}

	OutTargets.FirstPerson.ParentComponent = FirstPersonMesh;
	OutTargets.FirstPerson.SocketName = PresentationMapping->FirstPersonWieldedSocket;

	OutTargets.ThirdPerson.ParentComponent = GetMesh();
	OutTargets.ThirdPerson.SocketName = PresentationMapping->ThirdPersonWieldedSocket;

	return true;
}

void AEnclaveCharacter::LinkWieldedItemAnimLayers(const FGameplayTag& ItemType)
{
	const FEnclaveItemPresentationMapping* PresentationMapping =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindPresentationMapping(ItemType) : nullptr;

	if (!PresentationMapping)
	{
		return;
	}

	// Linking replaces whatever currently fills the same ALI layers, so a
	// straight item swap (rifle -> pistol) needs no explicit unlink first.
	if (IsValid(FirstPersonMesh) && PresentationMapping->FirstPersonAnimLayers)
	{
		FirstPersonMesh->LinkAnimClassLayers(PresentationMapping->FirstPersonAnimLayers);
	}

	if (IsValid(GetMesh()) && PresentationMapping->ThirdPersonAnimLayers)
	{
		GetMesh()->LinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
	}
}

void AEnclaveCharacter::UnlinkWieldedItemAnimLayers(const FGameplayTag& ItemType)
{
	const FEnclaveItemPresentationMapping* PresentationMapping =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindPresentationMapping(ItemType) : nullptr;

	if (!PresentationMapping)
	{
		return;
	}

	// Unlinking by class only resets layer nodes currently running that
	// class - if a newer item already linked over them this is a no-op,
	// otherwise the main ABP's own (default/unarmed) layers take back over.
	if (IsValid(FirstPersonMesh) && PresentationMapping->FirstPersonAnimLayers)
	{
		FirstPersonMesh->UnlinkAnimClassLayers(PresentationMapping->FirstPersonAnimLayers);
	}

	if (IsValid(GetMesh()) && PresentationMapping->ThirdPersonAnimLayers)
	{
		GetMesh()->UnlinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
	}
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
