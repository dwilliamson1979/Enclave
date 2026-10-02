// Copyright Lightcarrier Studios

#include "Characters/Components/EnclaveAvatarPresentationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/Notifies/EnclaveAnimNotify_ItemPresentationAction.h"
#include "Components/SkeletalMeshComponent.h"
#include "Debug/EnclaveDebug.h"
#include "Equipment/Data/EnclaveItemPresentationDefinition.h"
#include "Equipment/Items/EnclaveItem.h"
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
	if (!ShouldRunTransientPresentation())
	{
		return;
	}

	const FEnclaveItemPresentationMontages* Presentation =
		IsValid(ItemPresentationDefinition) ? ItemPresentationDefinition->FindItemPresentationMontages(Event.ItemType, Event.Action) : nullptr;

	const bool bChoreographedItemPresentation =
		Presentation && Presentation->ItemPresentationTiming == EEnclaveItemPresentationTiming::Choreographed;

	switch (Event.Phase)
	{
	case EEnclaveItemPresentationActionPhase::Started:
	{
		// Register notify-driven item choreography before starting the avatar
		// montage. A notify authored at montage time 0 can then safely resolve the
		// active item/action on its first animation update.
		if (bChoreographedItemPresentation)
		{
			BeginChoreographedItemAction(Event);
		}

		const bool bFirstPersonHasChoreographyNotify =
			bChoreographedItemPresentation && MontageContainsChoreographyNotify(Presentation->FirstPerson, Event.Action);
		const bool bThirdPersonHasChoreographyNotify =
			bChoreographedItemPresentation && MontageContainsChoreographyNotify(Presentation->ThirdPerson, Event.Action);

		const bool bFirstPersonPlayed = Presentation && PlayMontageOnMesh(FirstPersonMesh, Presentation->FirstPerson, Event.Duration);
		const bool bThirdPersonPlayed = Presentation && PlayMontageOnMesh(ThirdPersonMesh, Presentation->ThirdPerson, Event.Duration);

		if (bChoreographedItemPresentation)
		{
			const bool bHasViableChoreographyPath =
				(bFirstPersonHasChoreographyNotify && bFirstPersonPlayed) ||
				(bThirdPersonHasChoreographyNotify && bThirdPersonPlayed);

			if (!bHasViableChoreographyPath)
			{
				UE_LOG(LogEnclaveAvatarPresentation, Warning,
					TEXT("%s: choreographed presentation action %s for item type %s has no playable avatar montage containing a matching choreography notify; starting item presentation immediately."),
					*GetNameSafe(GetOwner()), *Event.Action.ToString(), *Event.ItemType.ToString());

				// Use the same idempotent path a real AnimNotify uses so duration
				// accounting and duplicate protection remain centralized.
				HandleItemPresentationChoreography(Event.Action);
			}
		}
		else if (IsValid(Event.Item))
		{
			// Immediate actions start their item-local presentation with the semantic action.
			Event.Item->StartPresentationAction(Event.Action, Event.Duration);
		}
		break;
	}

	case EEnclaveItemPresentationActionPhase::Ended:
		if (Presentation)
		{
			StopMontageOnMesh(FirstPersonMesh, Presentation->FirstPerson);
			StopMontageOnMesh(ThirdPersonMesh, Presentation->ThirdPerson);
		}

		if (bChoreographedItemPresentation)
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
	if (!ShouldRunTransientPresentation())
	{
		return;
	}

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

bool UEnclaveAvatarPresentationComponent::ShouldRunTransientPresentation() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_DedicatedServer;
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

bool UEnclaveAvatarPresentationComponent::MontageContainsChoreographyNotify(const UAnimMontage* Montage, const FGameplayTag& Action) const
{
	if (!IsValid(Montage) || !Action.IsValid())
	{
		return false;
	}

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		const UEnclaveAnimNotify_ItemPresentationAction* PresentationNotify =
			Cast<UEnclaveAnimNotify_ItemPresentationAction>(NotifyEvent.Notify.Get());

		if (PresentationNotify && PresentationNotify->GetAction().MatchesTagExact(Action))
		{
			return true;
		}
	}

	return false;
}

bool UEnclaveAvatarPresentationComponent::PlayMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage, float Duration) const
{
	UAnimInstance* AnimInstance = (IsValid(MeshComp) && Montage) ? MeshComp->GetAnimInstance() : nullptr;

	if (!AnimInstance)
	{
		return false;
	}

	const float MontageLength = Montage->GetPlayLength();
	const float PlayRate = (Duration > KINDA_SMALL_NUMBER && MontageLength > KINDA_SMALL_NUMBER) ? MontageLength / Duration : 1.0f;

	if (AnimInstance->Montage_Play(Montage, PlayRate) <= 0.0f)
	{
		UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: montage %s failed to play on %s - does its skeleton match the mesh's?"),
			*GetNameSafe(GetOwner()), *GetNameSafe(Montage), *GetNameSafe(MeshComp));
		return false;
	}

	return true;
}

void UEnclaveAvatarPresentationComponent::StopMontageOnMesh(USkeletalMeshComponent* MeshComp, UAnimMontage* Montage) const
{
	UAnimInstance* AnimInstance = (IsValid(MeshComp) && Montage) ? MeshComp->GetAnimInstance() : nullptr;

	if (AnimInstance && AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Stop(ActionEndedBlendOutTime, Montage);
	}
}

