// Copyright Lightcarrier Studios

#include "Equipment/Data/EnclaveItemPresentationDefinition.h"

#if WITH_EDITOR
#include "Animation/Notifies/EnclaveAnimNotify_ItemPresentationAction.h"
#include "Animation/AnimMontage.h"
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
namespace
{
bool MontageContainsPresentationActionNotify(const UAnimMontage* Montage, const FGameplayTag& Action)
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
}

EDataValidationResult UEnclaveItemPresentationDefinition::IsDataValid(FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);

	for (const TPair<FGameplayTag, FEnclaveItemPresentationMapping>& MappingPair : ItemPresentationMappings)
	{
		const FGameplayTag& ItemType = MappingPair.Key;

		for (const TPair<FGameplayTag, FEnclaveItemPresentationMontages>& ActionPair : MappingPair.Value.ItemActionMontages)
		{
			const FGameplayTag& Action = ActionPair.Key;
			const FEnclaveItemPresentationMontages& Montages = ActionPair.Value;

			if (Montages.ItemPresentationTiming != EEnclaveItemPresentationTiming::Choreographed)
			{
				continue;
			}

			const bool bFirstPersonHasNotify = MontageContainsPresentationActionNotify(Montages.FirstPerson, Action);
			const bool bThirdPersonHasNotify = MontageContainsPresentationActionNotify(Montages.ThirdPerson, Action);

			if (!bFirstPersonHasNotify && !bThirdPersonHasNotify)
			{
				Context.AddError(FText::Format(
					NSLOCTEXT("EnclaveItemPresentationDefinition", "MissingChoreographyNotify",
						"Item type {0}, action {1} is Choreographed, but neither avatar montage contains an Enclave Item Presentation Action notify for that action."),
					FText::FromString(ItemType.ToString()),
					FText::FromString(Action.ToString())));
			}
		}
	}

	return Context.GetNumErrors() > 0 ? EDataValidationResult::Invalid : EDataValidationResult::Valid;
}
#endif

const FEnclaveItemPresentationMapping* UEnclaveItemPresentationDefinition::FindPresentationMapping(const FGameplayTag& ItemType) const
{
	// Exact match first, then each parent in turn (Weapon.Type.Rifle.Marksman
	// -> Weapon.Type.Rifle -> Weapon.Type -> Weapon). RequestDirectParent
	// returns an invalid tag past the root, which ends the walk.
	for (FGameplayTag Tag = ItemType; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const FEnclaveItemPresentationMapping* Mapping = ItemPresentationMappings.Find(Tag))
		{
			return Mapping;
		}
	}

	return nullptr;
}

const FEnclaveItemPresentationMontages* UEnclaveItemPresentationDefinition::FindItemPresentationMontages(const FGameplayTag& ItemType, const FGameplayTag& Action) const
{
	const FEnclaveItemPresentationMapping* Mapping = FindPresentationMapping(ItemType);

	return (Mapping && Action.IsValid()) ? Mapping->ItemActionMontages.Find(Action) : nullptr;
}
