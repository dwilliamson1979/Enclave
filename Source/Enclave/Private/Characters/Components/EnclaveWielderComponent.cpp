// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveWielderComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Equipment/Data/EnclaveItemPresentationDefinition.h"
#include "Equipment/Items/EnclaveItem.h"
#include "Debug/EnclaveDebug.h"
#include "GameplayTags/EnclaveWielderTags.h"

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

	// Taking an item in hand IS drawing it - this body's own decision, so it
	// raises Draw itself rather than waiting to be told.
	FEnclaveWieldedItemActionEvent DrawEvent;
	DrawEvent.ItemType = Item->GetItemType();
	DrawEvent.Action = EnclaveGameplayTags::Wielder_Action_Draw;
	HandleWieldedItemAction(DrawEvent);

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

void UEnclaveWielderComponent::HandleWieldedItemAction(const FEnclaveWieldedItemActionEvent& Event)
{
	const FEnclaveWielderMontages* Montages =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindWielderMontages(Event.ItemType, Event.Action) : nullptr;

	if (!Montages)
	{
		// This body has nothing to show for this action with this item.
		return;
	}

	// Both meshes, every machine: owner-only / owner-no-see visibility
	// already decides which one a given viewer actually sees.
	switch (Event.Phase)
	{
	case EEnclaveWieldedItemActionPhase::Started:
		PlayMontageOnMesh(FirstPersonMesh, Montages->FirstPerson, Event.Duration);
		PlayMontageOnMesh(ThirdPersonMesh, Montages->ThirdPerson, Event.Duration);
		break;

	case EEnclaveWieldedItemActionPhase::Ended:
		// Only visible for an action cut short - a duration-scaled montage
		// that ran to completion has already finished.
		StopMontageOnMesh(FirstPersonMesh, Montages->FirstPerson);
		StopMontageOnMesh(ThirdPersonMesh, Montages->ThirdPerson);
		break;
	}
}

void UEnclaveWielderComponent::PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const
{
	UAnimInstance* AnimInstance = (IsValid(MeshComp) && Montage) ? MeshComp->GetAnimInstance() : nullptr;

	if (!AnimInstance)
	{
		return;
	}

	const float MontageLength = Montage->GetPlayLength();
	const float PlayRate = (Duration > KINDA_SMALL_NUMBER && MontageLength > KINDA_SMALL_NUMBER) ? MontageLength / Duration : 1.0f;

	if (AnimInstance->Montage_Play(Montage, PlayRate) <= 0.0f)
	{
		// Plain UE_LOG, not ENCLAVE_LOG: misconfigured data (almost always a
		// montage authored for a different skeleton than the mesh), which
		// otherwise fails completely silently.
		UE_LOG(LogEnclaveEquipment, Warning, TEXT("%s: montage %s failed to play on %s - does its skeleton match the mesh's?"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Montage), *GetNameSafe(MeshComp));
	}
}

void UEnclaveWielderComponent::StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const
{
	UAnimInstance* AnimInstance = (IsValid(MeshComp) && Montage) ? MeshComp->GetAnimInstance() : nullptr;

	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(ActionEndedBlendOutTime, Montage);
	}
}
