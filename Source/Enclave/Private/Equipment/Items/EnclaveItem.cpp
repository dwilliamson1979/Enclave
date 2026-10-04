// Copyright Lightcarrier Studios

#include "Equipment/Items/EnclaveItem.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Equipment/Data/EnclaveItemDefinition.h"

AEnclaveItem::AEnclaveItem()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bNetUseOwnerRelevancy = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(SceneRoot);
	FirstPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	FirstPersonMesh->SetReceivesDecals(false);
	FirstPersonMesh->SetCastShadow(false);
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetOwnerNoSee(false);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ThirdPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ThirdPersonMesh"));
	ThirdPersonMesh->SetupAttachment(SceneRoot);
	// Remote item montages can carry gameplay-relevant cosmetic notifies such as reload audio.
	// Keep montages advancing while off-screen without paying for full pose evaluation.
	ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	ThirdPersonMesh->SetReceivesDecals(false);
	ThirdPersonMesh->SetCastShadow(true);
	ThirdPersonMesh->SetOnlyOwnerSee(false);
	ThirdPersonMesh->SetOwnerNoSee(true);
	ThirdPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEnclaveItem::BeginPlay()
{
	Super::BeginPlay();

	ApplyItemDefinition();
}

const UEnclaveItemDefinition* AEnclaveItem::GetItemDefinition() const
{
	return ItemDefinition;
}

FGameplayTag AEnclaveItem::GetItemType() const
{
	const UEnclaveItemDefinition* Definition = GetItemDefinition();
	return IsValid(Definition) ? Definition->GetItemType() : FGameplayTag();
}

bool AEnclaveItem::HasItemCapability(const FGameplayTag& CapabilityTag) const
{
	const UEnclaveItemDefinition* Definition = GetItemDefinition();
	return IsValid(Definition) && Definition->HasItemCapability(CapabilityTag);
}

float AEnclaveItem::GetEquipDuration() const
{
	const UEnclaveItemDefinition* Definition = GetItemDefinition();
	return IsValid(Definition) ? Definition->GetEquipDuration() : 0.0f;
}

TArray<FEnclaveAbilityGrant> AEnclaveItem::GetAbilityGrants() const
{
	const UEnclaveItemDefinition* Definition = GetItemDefinition();
	return IsValid(Definition) ? Definition->GetAbilityGrants() : TArray<FEnclaveAbilityGrant>();
}

void AEnclaveItem::StartPresentationAction(const FGameplayTag& Action, float Duration)
{
}

void AEnclaveItem::EndPresentationAction(const FGameplayTag& Action)
{
}

void AEnclaveItem::ApplyItemDefinition()
{
	const UEnclaveItemDefinition* Definition = GetItemDefinition();

	if (!IsValid(Definition))
	{
		return;
	}

	FirstPersonMesh->SetSkeletalMesh(Definition->GetFirstPersonMesh());
	ThirdPersonMesh->SetSkeletalMesh(Definition->GetThirdPersonMesh());
}
