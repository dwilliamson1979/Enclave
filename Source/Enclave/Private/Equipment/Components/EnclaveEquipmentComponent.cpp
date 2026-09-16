// Copyright Lightcarrier Studios

#include "Equipment/Components/EnclaveEquipmentComponent.h"

#include "Components/SkeletalMeshComponent.h"

#include "Characters/EnclaveCharacter.h"

#include "Equipment/Data/EnclaveItemAttachmentDefinition.h"
#include "Equipment/Interfaces/EnclaveWieldableInterface.h"
#include "Equipment/Weapons/EnclaveWeapon.h"
#include "Equipment/Weapons/Data/EnclaveWeaponDefinition.h"

#include "GameFramework/Pawn.h"

#include "Net/UnrealNetwork.h"
#include "InputCoreTypes.h"

UEnclaveEquipmentComponent::UEnclaveEquipmentComponent()
{
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveEquipmentComponent::InitializeComponent()
{
	Super::InitializeComponent();

	RebuildInputFilters();
}

void UEnclaveEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UEnclaveEquipmentComponent, CurrentEquippedItem);
}

const FGameplayTagContainer& UEnclaveEquipmentComponent::GetInputFilters() const
{
	return InputFilters;
}

bool UEnclaveEquipmentComponent::HandleInput(const FEnclaveInputEvent& InputEvent)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Yellow,
			FString::Printf(TEXT("UEnclaveEquipmentComponent::HandleInput: %s"), *InputEvent.InputTag.ToString()));
	}

	if (NextEquipmentInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(NextEquipmentInputTag))
	{
		return HandleNextEquipmentInput(InputEvent);
	}

	if (PreviousEquipmentInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PreviousEquipmentInputTag))
	{
		return HandlePreviousEquipmentInput(InputEvent);
	}

	if (PrimaryItemInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(PrimaryItemInputTag))
	{
		return HandlePrimaryItemInput(InputEvent);
	}

	if (SecondaryItemInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(SecondaryItemInputTag))
	{
		return HandleSecondaryItemInput(InputEvent);
	}

	if (TertiaryItemInputTag.IsValid() && InputEvent.InputTag.MatchesTagExact(TertiaryItemInputTag))
	{
		return HandleTertiaryItemInput(InputEvent);
	}

	return false;
}

bool UEnclaveEquipmentComponent::TryEquipNext()
{
	if (!CanCycleEquipment())
	{
		return false;
	}

	// TODO: Select the next valid active equipment slot.

	return true;
}

bool UEnclaveEquipmentComponent::TryEquipPrevious()
{
	if (!CanCycleEquipment())
	{
		return false;
	}

	// TODO: Select the previous valid active equipment slot.

	return true;
}

bool UEnclaveEquipmentComponent::CanCycleEquipment() const
{
	// TODO: Add equipment-state validation once active slots and
	// equipment transitions are implemented.

	return true;
}

bool UEnclaveEquipmentComponent::TryPerformPrimaryAction(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentEquippedItem) || !CurrentEquippedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandlePrimaryAction(CurrentEquippedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformSecondaryAction(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentEquippedItem) || !CurrentEquippedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleSecondaryAction(CurrentEquippedItem, InputEvent);
}

bool UEnclaveEquipmentComponent::TryPerformTertiaryAction(const FEnclaveInputEvent& InputEvent)
{
	if (!IsValid(CurrentEquippedItem) || !CurrentEquippedItem->Implements<UEnclaveWieldableInterface>())
	{
		return false;
	}

	return IEnclaveWieldableInterface::Execute_HandleTertiaryAction(CurrentEquippedItem, InputEvent);
}

void UEnclaveEquipmentComponent::RebuildInputFilters()
{
	InputFilters.Reset();

	if (NextEquipmentInputTag.IsValid())
	{
		InputFilters.AddTag(NextEquipmentInputTag);
	}

	if (PreviousEquipmentInputTag.IsValid())
	{
		InputFilters.AddTag(PreviousEquipmentInputTag);
	}

	if (PrimaryItemInputTag.IsValid())
	{
		InputFilters.AddTag(PrimaryItemInputTag);
	}

	if (SecondaryItemInputTag.IsValid())
	{
		InputFilters.AddTag(SecondaryItemInputTag);
	}

	if (TertiaryItemInputTag.IsValid())
	{
		InputFilters.AddTag(TertiaryItemInputTag);
	}
}

bool UEnclaveEquipmentComponent::HandleNextEquipmentInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryEquipNext();
}

bool UEnclaveEquipmentComponent::HandlePreviousEquipmentInput(const FEnclaveInputEvent& InputEvent)
{
	if (InputEvent.TriggerEvent != ETriggerEvent::Started)
	{
		return false;
	}

	return TryEquipPrevious();
}

bool UEnclaveEquipmentComponent::HandlePrimaryItemInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformPrimaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleSecondaryItemInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformSecondaryAction(InputEvent);
}

bool UEnclaveEquipmentComponent::HandleTertiaryItemInput(const FEnclaveInputEvent& InputEvent)
{
	return TryPerformTertiaryAction(InputEvent);
}

AEnclaveWeapon* UEnclaveEquipmentComponent::SpawnDefaultWeapon(TSubclassOf<AEnclaveWeapon> WeaponClass)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !WeaponClass)
	{
		return nullptr;
	}

	if (IsValid(CurrentEquippedItem))
	{
		return Cast<AEnclaveWeapon>(CurrentEquippedItem);
	}

	UWorld* World = GetWorld();

	if (!IsValid(World))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = OwnerActor;
	SpawnParameters.Instigator = Cast<APawn>(OwnerActor);
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnclaveWeapon* Weapon = World->SpawnActor<AEnclaveWeapon>(WeaponClass, OwnerActor->GetActorTransform(), SpawnParameters);

	if (!IsValid(Weapon))
	{
		return nullptr;
	}

	CurrentEquippedItem = Weapon;

	AttachWeaponToOwner(Weapon);

	return Weapon;
}

bool UEnclaveEquipmentComponent::AttachWeaponToOwner(AEnclaveWeapon* Weapon)
{
	if (!IsValid(Weapon))
	{
		return false;
	}

	AEnclaveCharacter* Character = Cast<AEnclaveCharacter>(GetOwner());

	if (!IsValid(Character))
	{
		return false;
	}

	const UEnclaveWeaponDefinition* WeaponDefinition = Weapon->GetWeaponDefinition();

	if (!IsValid(WeaponDefinition))
	{
		return false;
	}

	const UEnclaveItemAttachmentDefinition* AttachmentDefinition = Character->GetItemAttachmentDefinition();

	if (!IsValid(AttachmentDefinition))
	{
		return false;
	}

	const FEnclaveItemAttachmentMapping* AttachmentMapping = AttachmentDefinition->FindAttachmentMapping(WeaponDefinition->GetWeaponType());

	if (!AttachmentMapping)
	{
		return false;
	}

	USkeletalMeshComponent* FirstPersonCharacterMesh = Character->GetFirstPersonMesh();
	USkeletalMeshComponent* ThirdPersonCharacterMesh = Character->GetMesh();

	if (!IsValid(FirstPersonCharacterMesh) || !IsValid(ThirdPersonCharacterMesh))
	{
		return false;
	}

	Weapon->AttachToActor(Character, FAttachmentTransformRules::KeepWorldTransform);

	const bool bFirstPersonAttached = Weapon->GetFirstPersonMesh()->AttachToComponent(
		FirstPersonCharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachmentMapping->WieldedSocket);

	const bool bThirdPersonAttached = Weapon->GetThirdPersonMesh()->AttachToComponent(
		ThirdPersonCharacterMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		AttachmentMapping->WieldedSocket);

	return bFirstPersonAttached && bThirdPersonAttached;
}

void UEnclaveEquipmentComponent::OnRep_CurrentEquippedItem()
{
	AEnclaveWeapon* Weapon = Cast<AEnclaveWeapon>(CurrentEquippedItem);

	if (IsValid(Weapon))
	{
		AttachWeaponToOwner(Weapon);
	}
}