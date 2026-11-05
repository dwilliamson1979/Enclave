// Copyright Lightcarrier Studios

#include "Animation/EnclaveGameplayTagPropertyMap.h"

#include "AbilitySystemComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "Debug/EnclaveDebug.h"

FEnclaveGameplayTagPropertyMap::~FEnclaveGameplayTagPropertyMap()
{
	// A safety net: owners unbind explicitly, long before destruction.
	Unbind();
}

FEnclaveGameplayTagPropertyMap::FEnclaveGameplayTagPropertyMap(const FEnclaveGameplayTagPropertyMap& Other)
	: Bindings(Other.Bindings)
{
}

FEnclaveGameplayTagPropertyMap& FEnclaveGameplayTagPropertyMap::operator=(const FEnclaveGameplayTagPropertyMap& Other)
{
	if (this != &Other)
	{
		Unbind();
		Bindings = Other.Bindings;
	}
	return *this;
}

void FEnclaveGameplayTagPropertyMap::Bind(UObject* Owner, UAbilitySystemComponent* ASC)
{
	Unbind();

	if (!IsValid(Owner) || !IsValid(ASC))
	{
		return;
	}

	BoundAbilitySystemComponent = ASC;

	for (const FEnclaveGameplayTagPropertyBinding& Binding : Bindings)
	{
		FProperty* Property = FindSupportedProperty(Owner->GetClass(), Binding.PropertyName);
		if (!Binding.Tag.IsValid() || !Property)
		{
			// Data validation reports these in the editor; skip at runtime.
			UE_LOG(LogEnclaveAvatarPresentation, Warning, TEXT("%s: skipping tag binding %s -> %s (no tag, or no bool/int/float property of that name)."),
				*GetNameSafe(Owner->GetClass()), *Binding.Tag.ToString(), *Binding.PropertyName.ToString());
			continue;
		}

		// A bool only cares about present/absent; counts need every change.
		const EGameplayTagEventType::Type EventType = Property->IsA<FBoolProperty>()
			? EGameplayTagEventType::NewOrRemoved
			: EGameplayTagEventType::AnyCountChange;

		// Weak on Owner: the delegate never runs once Owner is gone, even if
		// Unbind was somehow missed. Property belongs to Owner's class.
		const FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Binding.Tag, EventType).AddWeakLambda(Owner,
			[Owner, Property](const FGameplayTag, int32 NewCount)
			{
				SetPropertyFromCount(Owner, Property, NewCount);
			});

		ActiveBindings.Add({ Binding.Tag, EventType, Handle });
		SetPropertyFromCount(Owner, Property, ASC->GetTagCount(Binding.Tag));
	}
}

void FEnclaveGameplayTagPropertyMap::Unbind()
{
	// If the ASC is already gone, its delegates went with it.
	if (UAbilitySystemComponent* ASC = BoundAbilitySystemComponent.Get())
	{
		for (const FActiveBinding& Active : ActiveBindings)
		{
			ASC->UnregisterGameplayTagEvent(Active.Handle, Active.Tag, Active.EventType);
		}
	}

	ActiveBindings.Reset();
	BoundAbilitySystemComponent.Reset();
}

FProperty* FEnclaveGameplayTagPropertyMap::FindSupportedProperty(const UClass* OwnerClass, FName PropertyName)
{
	FProperty* Property = OwnerClass ? OwnerClass->FindPropertyByName(PropertyName) : nullptr;
	const bool bSupported = Property
		&& (Property->IsA<FBoolProperty>() || Property->IsA<FIntProperty>()
			|| Property->IsA<FFloatProperty>() || Property->IsA<FDoubleProperty>());
	return bSupported ? Property : nullptr;
}

void FEnclaveGameplayTagPropertyMap::SetPropertyFromCount(UObject* Owner, const FProperty* Property, int32 Count)
{
	if (const FBoolProperty* BoolProperty = CastField<const FBoolProperty>(Property))
	{
		BoolProperty->SetPropertyValue_InContainer(Owner, Count > 0);
	}
	else if (const FIntProperty* IntProperty = CastField<const FIntProperty>(Property))
	{
		IntProperty->SetPropertyValue_InContainer(Owner, Count);
	}
	else if (const FFloatProperty* FloatProperty = CastField<const FFloatProperty>(Property))
	{
		FloatProperty->SetPropertyValue_InContainer(Owner, static_cast<float>(Count));
	}
	else if (const FDoubleProperty* DoubleProperty = CastField<const FDoubleProperty>(Property))
	{
		DoubleProperty->SetPropertyValue_InContainer(Owner, static_cast<double>(Count));
	}
}

#if WITH_EDITOR
void FEnclaveGameplayTagPropertyMap::ValidateBindings(const UClass* OwnerClass, FDataValidationContext& Context) const
{
	for (const FEnclaveGameplayTagPropertyBinding& Binding : Bindings)
	{
		if (!Binding.Tag.IsValid())
		{
			Context.AddError(FText::Format(NSLOCTEXT("Enclave", "TagBindingNoTag", "A gameplay tag binding to [{0}] has no tag."),
				FText::FromName(Binding.PropertyName)));
		}
		if (!FindSupportedProperty(OwnerClass, Binding.PropertyName))
		{
			Context.AddError(FText::Format(NSLOCTEXT("Enclave", "TagBindingBadProperty",
				"The gameplay tag binding for [{0}] names [{1}], which is not a bool, int, float or double variable."),
				FText::AsCultureInvariant(Binding.Tag.ToString()), FText::FromName(Binding.PropertyName)));
		}
	}
}
#endif
