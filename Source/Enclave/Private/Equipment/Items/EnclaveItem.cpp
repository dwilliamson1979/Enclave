// Copyright Lightcarrier Studios

#include "Equipment/Items/EnclaveItem.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

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

FGameplayTag AEnclaveItem::GetItemType() const
{
	return FGameplayTag();
}

bool AEnclaveItem::HasItemCapability(const FGameplayTag&) const
{
	return false;
}

float AEnclaveItem::GetEquipDuration() const
{
	return 0.0f;
}

void AEnclaveItem::StartPresentationAction(const FGameplayTag& Action, float Duration)
{
}

void AEnclaveItem::EndPresentationAction(const FGameplayTag& Action)
{
}
