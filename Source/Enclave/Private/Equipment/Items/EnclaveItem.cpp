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
	ThirdPersonMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
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