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

void AEnclaveItem::NotifyWielderOfAction(const FGameplayTag& Action, EEnclaveWieldedItemActionPhase Phase, float Duration) const
{
	// The wielder attaches this item's actor to its own actor when it wields
	// it, so the attach parent is where the wielder lives.
	IEnclaveItemWielderInterface* Wielder = IEnclaveItemWielderInterface::FindWielder(GetAttachParentActor());

	if (!Wielder)
	{
		return;
	}

	FEnclaveWieldedItemActionEvent Event;
	Event.ItemType = GetItemType();
	Event.Action = Action;
	Event.Phase = Phase;
	Event.Duration = Duration;

	Wielder->HandleWieldedItemAction(Event);
}
