// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveAvatarPresentationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/EnclaveDebug.h"
#include "Equipment/Data/EnclaveItemPresentationDefinition.h"
#include "Equipment/Items/EnclaveItem.h"

UEnclaveAvatarPresentationComponent::UEnclaveAvatarPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnclaveAvatarPresentationComponent::SetPresentationMeshes(USkeletalMeshComponent* InFirstPersonMesh, USkeletalMeshComponent* InThirdPersonMesh)
{
	FirstPersonMesh = InFirstPersonMesh;
	ThirdPersonMesh = InThirdPersonMesh;
}

void UEnclaveAvatarPresentationComponent::PresentWieldedItem(AEnclaveItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	// Keep a failed presentation invisible rather than leaving the item at its
	// previous/world transform. Presentation failure does not alter equipment state.
	Item->SetActorHiddenInGame(true);

	if (!AttachItem(Item))
	{
		return;
	}

	LinkAnimLayers(Item->GetItemType());
	Item->SetActorHiddenInGame(false);
}

void UEnclaveAvatarPresentationComponent::ClearWieldedItemPresentation(AEnclaveItem* Item)
{
	if (!IsValid(Item))
	{
		return;
	}

	UnlinkAnimLayers(Item->GetItemType());
	Item->SetActorHiddenInGame(true);
}


void UEnclaveAvatarPresentationComponent::HandleItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event)
{
	const FEnclaveItemPresentationMontages* Montages =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindItemPresentationMontages(Event.ItemType, Event.Action) : nullptr;

	switch (Event.Phase)
	{
	case EEnclaveItemPresentationActionPhase::Started:
		// Phase 2: AvatarPresentation is now the single coordinator for both
		// sides of an item action. Item-local presentation still starts
		// immediately for now; a later choreography phase can move this call
		// behind an authored AnimNotify without changing the gameplay cue path.
		if (Montages)
		{
			PlayMontageOnMesh(FirstPersonMesh, Montages->FirstPerson, Event.Duration);
			PlayMontageOnMesh(ThirdPersonMesh, Montages->ThirdPerson, Event.Duration);
		}

		if (IsValid(Event.Item))
		{
			Event.Item->StartPresentationAction(Event.Action, Event.Duration);
		}
		break;

	case EEnclaveItemPresentationActionPhase::Ended:
		if (Montages)
		{
			StopMontageOnMesh(FirstPersonMesh, Montages->FirstPerson);
			StopMontageOnMesh(ThirdPersonMesh, Montages->ThirdPerson);
		}

		if (IsValid(Event.Item))
		{
			Event.Item->EndPresentationAction(Event.Action);
		}
		break;
	}
}

bool UEnclaveAvatarPresentationComponent::AttachItem(AEnclaveItem* Item)
{
	AActor* OwnerActor = GetOwner();

	if (!IsValid(OwnerActor) || !IsValid(Item) || !IsValid(FirstPersonMesh) || !IsValid(ThirdPersonMesh) || !IsValid(ItemPresentationDefinition))
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: unable to present %s because avatar presentation is not fully configured."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item));
		return false;
	}

	USkeletalMeshComponent* ItemFirstPersonMesh = Item->GetFirstPersonMesh();
	USkeletalMeshComponent* ItemThirdPersonMesh = Item->GetThirdPersonMesh();

	if (!IsValid(ItemFirstPersonMesh) || !IsValid(ItemThirdPersonMesh))
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: unable to present %s because its presentation meshes are invalid."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item));
		return false;
	}

	const FEnclaveItemPresentationMapping* PresentationMapping = ItemPresentationDefinition->FindPresentationMapping(Item->GetItemType());

	if (!PresentationMapping || PresentationMapping->FirstPersonWieldedSocket.IsNone() || PresentationMapping->ThirdPersonWieldedSocket.IsNone())
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: unable to present %s because no valid attachment mapping exists for item type %s."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item), *Item->GetItemType().ToString());
		return false;
	}

	if (!FirstPersonMesh->DoesSocketExist(PresentationMapping->FirstPersonWieldedSocket) ||
		!ThirdPersonMesh->DoesSocketExist(PresentationMapping->ThirdPersonWieldedSocket))
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: unable to present %s because one or more wielded sockets do not exist (FP=%s, TP=%s)."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item), *PresentationMapping->FirstPersonWieldedSocket.ToString(),
			*PresentationMapping->ThirdPersonWieldedSocket.ToString());
		return false;
	}

	if (!Item->AttachToActor(OwnerActor, FAttachmentTransformRules::KeepWorldTransform))
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: failed to attach item actor %s to the avatar."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item));
		return false;
	}

	const bool bFirstPersonAttached = ItemFirstPersonMesh->AttachToComponent(
		FirstPersonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, PresentationMapping->FirstPersonWieldedSocket);

	if (!bFirstPersonAttached)
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: failed to attach %s first-person mesh to socket %s."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item), *PresentationMapping->FirstPersonWieldedSocket.ToString());
	}

	const bool bThirdPersonAttached = ItemThirdPersonMesh->AttachToComponent(
		ThirdPersonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, PresentationMapping->ThirdPersonWieldedSocket);

	if (!bThirdPersonAttached)
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: failed to attach %s third-person mesh to socket %s."),
			*GetNameSafe(OwnerActor), *GetNameSafe(Item), *PresentationMapping->ThirdPersonWieldedSocket.ToString());
	}

	return bFirstPersonAttached && bThirdPersonAttached;
}

void UEnclaveAvatarPresentationComponent::LinkAnimLayers(const FGameplayTag& ItemType)
{
	const FEnclaveItemPresentationMapping* PresentationMapping =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindPresentationMapping(ItemType) : nullptr;

	if (!PresentationMapping)
	{
		return;
	}

	if (IsValid(FirstPersonMesh) && PresentationMapping->FirstPersonAnimLayers)
	{
		FirstPersonMesh->LinkAnimClassLayers(PresentationMapping->FirstPersonAnimLayers);
	}

	if (IsValid(ThirdPersonMesh) && PresentationMapping->ThirdPersonAnimLayers)
	{
		ThirdPersonMesh->LinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
	}
}

void UEnclaveAvatarPresentationComponent::UnlinkAnimLayers(const FGameplayTag& ItemType)
{
	const FEnclaveItemPresentationMapping* PresentationMapping =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindPresentationMapping(ItemType) : nullptr;

	if (!PresentationMapping)
	{
		return;
	}

	if (IsValid(FirstPersonMesh) && PresentationMapping->FirstPersonAnimLayers)
	{
		FirstPersonMesh->UnlinkAnimClassLayers(PresentationMapping->FirstPersonAnimLayers);
	}

	if (IsValid(ThirdPersonMesh) && PresentationMapping->ThirdPersonAnimLayers)
	{
		ThirdPersonMesh->UnlinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
	}
}

void UEnclaveAvatarPresentationComponent::PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const
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
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: montage %s failed to play on %s - does its skeleton match the mesh's?"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Montage), *GetNameSafe(MeshComp));
	}
}

void UEnclaveAvatarPresentationComponent::StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const
{
	UAnimInstance* AnimInstance = (IsValid(MeshComp) && Montage) ? MeshComp->GetAnimInstance() : nullptr;

	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(ActionEndedBlendOutTime, Montage);
	}
}

