// Copyright Lightcarrier Studios


#include "Characters/EnclaveCharacter.h"

#include "Camera/CameraComponent.h"

#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Input/Components/EnclaveInputRouterComponent.h"
#include "Input/Core/EnclaveInputConfig.h"

#include "Characters/Components/EnclaveCharacterMovementComponent.h"
#include "Characters/Components/EnclaveViewComponent.h"
#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Combat/EnclaveCombatComponent.h"

#include "Equipment/Data/EnclaveItemAttachmentDefinition.h"

#include "Engine/Engine.h"

AEnclaveCharacter::AEnclaveCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEnclaveCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// Camera boom
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>("SpringArmComp");
	SpringArmComp->SetupAttachment(GetRootComponent());
	SpringArmComp->TargetArmLength = 0.0f;
	SpringArmComp->bEnableCameraLag = true;
	SpringArmComp->CameraLagSpeed = 15.f;
	SpringArmComp->bUsePawnControlRotation = true;

	//What if in the future we want the player to be able to alternate between first and third person views?

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

	CombatComp = CreateDefaultSubobject<UEnclaveCombatComponent>(TEXT("CombatComp"));
	CombatComp->SetIsReplicated(true);

	EquipmentComp = CreateDefaultSubobject<UEnclaveEquipmentComponent>(TEXT("EquipmentComp"));
	EquipmentComp->SetIsReplicated(true);
}

// Called when the game starts or when spawned
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

const UEnclaveItemAttachmentDefinition* AEnclaveCharacter::GetItemAttachmentDefinition() const
{
	return ItemAttachmentDefinition;
}