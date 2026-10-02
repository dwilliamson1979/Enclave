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

	// Presentation owns the persistent state it applied. If another caller asks
	// us to present a different item without clearing first, tear down the old
	// state here so cleanup never depends on the old gameplay object surviving.
	if (PresentedWieldedItemState.IsActive())
	{
		if (PresentedWieldedItemState.Item.Get() == Item)
		{
			return;
		}

		ClearWieldedItemPresentation();
	}

	// Keep a failed presentation invisible rather than leaving the item at its
	// previous/world transform. Presentation failure does not alter equipment state.
	Item->SetActorHiddenInGame(true);

	if (!AttachItem(Item))
	{
		return;
	}

	PresentedWieldedItemState.Item = Item;
	PresentedWieldedItemState.ItemType = Item->GetItemType();
	PresentedWieldedItemState.bIsActive = true;

	LinkAnimLayers(PresentedWieldedItemState.ItemType);
	Item->SetActorHiddenInGame(false);
}

void UEnclaveAvatarPresentationComponent::ClearWieldedItemPresentation()
{
	// Active item actions belong to the presentation being removed. End them
	// first so lasting Immediate actions are cleaned up too, and late notifies
	// from old choreographed montages cannot affect the next item.
	EndAllActiveItemPresentationActions();

	if (!PresentedWieldedItemState.IsActive())
	{
		return;
	}

	UnlinkPresentedAnimLayers();

	if (AEnclaveItem* Item = PresentedWieldedItemState.Item.Get(); IsValid(Item))
	{
		Item->SetActorHiddenInGame(true);
	}

	PresentedWieldedItemState.Reset();
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
		// Duration actions need deterministic teardown even when their item-local
		// presentation starts immediately. Choreographed one-shots also need a
		// short-lived record while waiting for their authored montage notify.
		const bool bShouldTrackAction =
			Event.Lifecycle == EEnclaveItemPresentationActionLifecycle::Duration || bChoreographedItemPresentation;

		if (bShouldTrackAction)
		{
			BeginActiveItemPresentationAction(Event, Presentation, bChoreographedItemPresentation);
		}

		const bool bFirstPersonHasChoreographyNotify =
			bChoreographedItemPresentation &&
			UEnclaveAnimNotify_ItemPresentationAction::MontageContainsAction(Presentation->FirstPerson, Event.Action);
		const bool bThirdPersonHasChoreographyNotify =
			bChoreographedItemPresentation &&
			UEnclaveAnimNotify_ItemPresentationAction::MontageContainsAction(Presentation->ThirdPerson, Event.Action);

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
					TEXT("%s: choreographed presentation action %s for item type %s has no playable avatar montage containing a matching montage-authored choreography notify; starting item presentation immediately."),
					*GetNameSafe(GetOwner()), *Event.Action.ToString(), *Event.ItemType.ToString());

				// Use the same idempotent path a real AnimNotify uses so duration
				// accounting and duplicate protection remain centralized.
				HandleItemPresentationChoreography(Event.Action);
			}
		}
		else if (IsValid(Event.Item))
		{
			Event.Item->StartPresentationAction(Event.Action, Event.Duration);

			if (FActiveItemPresentationAction* ActiveAction = ActiveItemPresentationActions.Find(Event.Action))
			{
				ActiveAction->bItemPresentationStarted = true;
			}
		}
		break;
	}

	case EEnclaveItemPresentationActionPhase::Ended:
	{
		FActiveItemPresentationAction* ActiveAction = ActiveItemPresentationActions.Find(Event.Action);

		if (!ActiveAction)
		{
			// Duration Starts are always tracked. An untracked Ended event is therefore
			// stale/out-of-order and must not tear down presentation for a newer item.
			break;
		}

		AEnclaveItem* ActiveItem = ActiveAction->Item.Get();

		if ((IsValid(Event.Item) && ActiveItem != Event.Item.Get()) ||
			(!IsValid(Event.Item) && IsValid(ActiveItem)))
		{
			// A late Removed from a previously wielded item can arrive after the same
			// semantic action has already started on the replacement item. Never let
			// that old lifecycle event terminate the new presentation instance.
			break;
		}

		EndActiveItemPresentationAction(Event.Action);
		break;
	}
	}
}

void UEnclaveAvatarPresentationComponent::HandleItemPresentationChoreography(const FGameplayTag& Action)
{
	if (!ShouldRunTransientPresentation())
	{
		return;
	}

	FActiveItemPresentationAction* ActiveAction = ActiveItemPresentationActions.Find(Action);

	if (!ActiveAction || !ActiveAction->bChoreographed || ActiveAction->bItemPresentationStarted)
	{
		return;
	}

	AEnclaveItem* Item = ActiveAction->Item.Get();

	if (!IsValid(Item))
	{
		if (ActiveAction->Lifecycle == EEnclaveItemPresentationActionLifecycle::OneShot)
		{
			ActiveItemPresentationActions.Remove(Action);
		}
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
		// not start an item montage at an extreme play rate just because a late
		// notify arrived. Duration actions remain tracked until Ended/teardown;
		// one-shots have no later lifecycle event and can be released now.
		if (RemainingDuration <= KINDA_SMALL_NUMBER)
		{
			ActiveAction->bItemPresentationStarted = true;

			if (ActiveAction->Lifecycle == EEnclaveItemPresentationActionLifecycle::OneShot)
			{
				ActiveItemPresentationActions.Remove(Action);
			}
			return;
		}
	}

	// FP and TP avatar montages may both carry the same semantic notify so the
	// visible mesh drives choreography for both owners and simulated proxies.
	// Mark started before dispatch so duplicate notifies are idempotent.
	ActiveAction->bItemPresentationStarted = true;
	Item->StartPresentationAction(Action, RemainingDuration);

	// A choreographed one-shot only needs tracking while waiting for its first
	// valid notify. Duration actions stay active so item swap/death can end the
	// item-local portion even if the cue's Removed event never arrives.
	if (ActiveAction->Lifecycle == EEnclaveItemPresentationActionLifecycle::OneShot)
	{
		ActiveItemPresentationActions.Remove(Action);
	}
}

void UEnclaveAvatarPresentationComponent::BeginActiveItemPresentationAction(const FEnclaveItemPresentationActionEvent& Event, const FEnclaveItemPresentationMontages* Presentation, bool bChoreographed)
{
	// Re-starting the same semantic action replaces any stale presentation
	// record rather than inheriting teardown state from the previous instance.
	EndActiveItemPresentationAction(Event.Action);

	FActiveItemPresentationAction& ActiveAction = ActiveItemPresentationActions.Add(Event.Action);
	ActiveAction.Item = Event.Item;
	ActiveAction.FirstPersonMontage = Presentation ? Presentation->FirstPerson.Get() : nullptr;
	ActiveAction.ThirdPersonMontage = Presentation ? Presentation->ThirdPerson.Get() : nullptr;
	ActiveAction.Lifecycle = Event.Lifecycle;
	ActiveAction.Duration = Event.Duration;
	ActiveAction.StartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ActiveAction.bChoreographed = bChoreographed;
	ActiveAction.bItemPresentationStarted = false;
}

bool UEnclaveAvatarPresentationComponent::EndActiveItemPresentationAction(const FGameplayTag& Action)
{
	FActiveItemPresentationAction* ActiveAction = ActiveItemPresentationActions.Find(Action);

	if (!ActiveAction)
	{
		return false;
	}

	StopMontageOnMesh(FirstPersonMesh, ActiveAction->FirstPersonMontage.Get());
	StopMontageOnMesh(ThirdPersonMesh, ActiveAction->ThirdPersonMontage.Get());

	if (ActiveAction->bItemPresentationStarted)
	{
		if (AEnclaveItem* Item = ActiveAction->Item.Get(); IsValid(Item))
		{
			Item->EndPresentationAction(Action);
		}
	}

	ActiveItemPresentationActions.Remove(Action);
	return true;
}

void UEnclaveAvatarPresentationComponent::EndAllActiveItemPresentationActions()
{
	TArray<FGameplayTag> ActiveActions;
	ActiveItemPresentationActions.GenerateKeyArray(ActiveActions);

	for (const FGameplayTag& Action : ActiveActions)
	{
		EndActiveItemPresentationAction(Action);
	}
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
		PresentedWieldedItemState.FirstPersonAnimLayers = PresentationMapping->FirstPersonAnimLayers;
	}

	if (IsValid(ThirdPersonMesh) && PresentationMapping->ThirdPersonAnimLayers)
	{
		ThirdPersonMesh->LinkAnimClassLayers(PresentationMapping->ThirdPersonAnimLayers);
		PresentedWieldedItemState.ThirdPersonAnimLayers = PresentationMapping->ThirdPersonAnimLayers;
	}
}

void UEnclaveAvatarPresentationComponent::UnlinkPresentedAnimLayers()
{
	if (IsValid(FirstPersonMesh) && PresentedWieldedItemState.FirstPersonAnimLayers)
	{
		FirstPersonMesh->UnlinkAnimClassLayers(PresentedWieldedItemState.FirstPersonAnimLayers);
	}

	if (IsValid(ThirdPersonMesh) && PresentedWieldedItemState.ThirdPersonAnimLayers)
	{
		ThirdPersonMesh->UnlinkAnimClassLayers(PresentedWieldedItemState.ThirdPersonAnimLayers);
	}
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

