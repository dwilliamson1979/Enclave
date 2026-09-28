// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveWielderComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Equipment/Data/EnclaveItemPresentationDefinition.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Debug/EnclaveDebug.h"

UEnclaveWielderComponent::UEnclaveWielderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveWielderComponent::SetWielderMeshes(USkeletalMeshComponent* InFirstPersonMesh, USkeletalMeshComponent* InThirdPersonMesh)
{
	FirstPersonMesh = InFirstPersonMesh;
	ThirdPersonMesh = InThirdPersonMesh;
}

bool UEnclaveWielderComponent::WieldItem(AEnclaveItem* Item)
{
	if (!IsValid(Item) || !AttachItem(Item))
	{
		return false;
	}

	Item->SetActorHiddenInGame(false);
	LinkAnimLayers(Item->GetItemType());

	return true;
}

void UEnclaveWielderComponent::UnwieldItem(AEnclaveItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	UnlinkAnimLayers(Item->GetItemType());

	// No stowing yet (the presentation row's StowedSocket is unused) - an
	// item out of hand is simply hidden where it is.
	Item->SetActorHiddenInGame(true);
}

bool UEnclaveWielderComponent::AttachItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !IsValid(FirstPersonMesh) || !IsValid(ThirdPersonMesh) || !IsValid(ItemPresentationDefinition))
	{
		// Plain UE_LOG: this is misconfiguration (meshes not handed over by
		// the owner, or no presentation asset assigned), not a normal case.
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: wielder is missing its meshes or ItemPresentationDefinition - can't wield %s"),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item));
		return false;
	}

	USkeletalMeshComponent* ItemFirstPersonMesh = Item->GetFirstPersonMesh();
	USkeletalMeshComponent* ItemThirdPersonMesh = Item->GetThirdPersonMesh();

	if (!IsValid(ItemFirstPersonMesh) || !IsValid(ItemThirdPersonMesh))
	{
		return false;
	}

	const FEnclaveItemPresentationMapping* PresentationMapping = ItemPresentationDefinition->FindPresentationMapping(Item->GetItemType());

	if (!PresentationMapping ||
		PresentationMapping->FirstPersonWieldedSocket.IsNone() ||
		PresentationMapping->ThirdPersonWieldedSocket.IsNone())
	{
		return false;
	}

	// The item actor itself follows the owner (so the item's
	// GetAttachParentActor resolves back to its wielder's actor); its meshes
	// then snap to this body's own sockets for each view.
	Item->AttachToActor(OwnerActor, FAttachmentTransformRules::KeepWorldTransform);

	const bool bFirstPersonAttached = ItemFirstPersonMesh->AttachToComponent(
		FirstPersonMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		PresentationMapping->FirstPersonWieldedSocket);

	const bool bThirdPersonAttached = ItemThirdPersonMesh->AttachToComponent(
		ThirdPersonMesh,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		PresentationMapping->ThirdPersonWieldedSocket);

	return bFirstPersonAttached && bThirdPersonAttached;
}

void UEnclaveWielderComponent::LinkAnimLayers(const FGameplayTag& ItemType)
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

	if (IsValid(ThirdPersonMesh) && PresentationMapping->ThirdPersonAnimLayers)
	{
		ThirdPersonMesh->LinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
	}
}

void UEnclaveWielderComponent::UnlinkAnimLayers(const FGameplayTag& ItemType)
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

	if (IsValid(ThirdPersonMesh) && PresentationMapping->ThirdPersonAnimLayers)
	{
		ThirdPersonMesh->UnlinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
	}
}
