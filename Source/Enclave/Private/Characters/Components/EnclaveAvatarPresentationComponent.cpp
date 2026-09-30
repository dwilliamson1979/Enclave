// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveAvatarPresentationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/EnclaveDebug.h"
#include "Equipment/Data/EnclaveItemPresentationDefinition.h"
#include "Equipment/Items/EnclaveItem.h"
#include "GameplayTags/EnclavePresentationTags.h"
#include "Engine/World.h"

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
		// Register any notify-driven item choreography before starting the avatar
		// montage. A notify authored at montage time 0 can then safely resolve the
		// active item/action on its first animation update.
		if (ShouldDeferItemPresentationToChoreography(Event.Action))
		{
			BeginChoreographedItemAction(Event);
		}

		if (Montages)
		{
			PlayMontageOnMesh(FirstPersonMesh, Montages->FirstPerson, Event.Duration);
			PlayMontageOnMesh(ThirdPersonMesh, Montages->ThirdPerson, Event.Duration);
		}

		// Instant/non-choreographed actions still start their item-local
		// presentation immediately. Reload is deliberately excluded here: its
		// weapon animation now begins from an authored avatar AnimNotify.
		if (!ShouldDeferItemPresentationToChoreography(Event.Action) && IsValid(Event.Item))
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

		if (ShouldDeferItemPresentationToChoreography(Event.Action))
		{
			EndChoreographedItemAction(Event.Action);
		}
		else if (IsValid(Event.Item))
		{
			Event.Item->EndPresentationAction(Event.Action);
		}
		break;
	}
}

void UEnclaveAvatarPresentationComponent::HandleItemPresentationChoreography(const FGameplayTag& Action)
{
	FActiveChoreographedItemAction* ActiveAction = ActiveChoreographedItemActions.Find(Action);

	if (!ActiveAction || ActiveAction->bItemPresentationStarted)
	{
		return;
	}

	AEnclaveItem* Item = ActiveAction->Item.Get();

	if (!IsValid(Item))
	{
		return;
	}

	float RemainingDuration = ActiveAction->Duration;

	if (RemainingDuration > KINDA_SMALL_NUMBER)
	{
		const UWorld* World = GetWorld();
		const float CurrentTimeSeconds = World ? World->GetTimeSeconds() : ActiveAction->StartTimeSeconds;
		const float ElapsedDuration = FMath::Max(0.0f, CurrentTimeSeconds - ActiveAction->StartTimeSeconds);
		RemainingDuration = FMath::Max(0.0f, ActiveAction->Duration - ElapsedDuration);

		// If the gameplay-owned presentation window has effectively ended, do
		// not start a montage at an extreme play rate just because a late notify
		// arrived. The Ended event will clean up the tracked action normally.
		if (RemainingDuration <= KINDA_SMALL_NUMBER)
		{
			ActiveAction->bItemPresentationStarted = true;
			return;
		}
	}

	// FP and TP avatar montages may both carry the same semantic notify so the
	// visible mesh drives choreography for both owners and simulated proxies.
	// Mark started before dispatch so duplicate notifies are idempotent.
	ActiveAction->bItemPresentationStarted = true;
	Item->StartPresentationAction(Action, RemainingDuration);
}

bool UEnclaveAvatarPresentationComponent::ShouldDeferItemPresentationToChoreography(const FGameplayTag& Action) const
{
	// Phase 3 intentionally proves the choreography path with Reload only.
	// Fire/DryFire remain immediate, and future coordinated actions can opt in
	// deliberately rather than making every item action depend on a notify.
	return Action.MatchesTagExact(EnclaveGameplayTags::Presentation_Item_Action_Reload);
}

void UEnclaveAvatarPresentationComponent::BeginChoreographedItemAction(const FEnclaveItemPresentationActionEvent& Event)
{
	FActiveChoreographedItemAction& ActiveAction = ActiveChoreographedItemActions.FindOrAdd(Event.Action);
	ActiveAction.Item = Event.Item;
	ActiveAction.Duration = Event.Duration;
	ActiveAction.StartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ActiveAction.bItemPresentationStarted = false;
}

void UEnclaveAvatarPresentationComponent::EndChoreographedItemAction(const FGameplayTag& Action)
{
	FActiveChoreographedItemAction* ActiveAction = ActiveChoreographedItemActions.Find(Action);

	if (!ActiveAction)
	{
		return;
	}

	if (ActiveAction->bItemPresentationStarted)
	{
		if (AEnclaveItem* Item = ActiveAction->Item.Get(); IsValid(Item))
		{
			Item->EndPresentationAction(Action);
		}
	}

	ActiveChoreographedItemActions.Remove(Action);
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

