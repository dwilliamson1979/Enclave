// Copyright Lightcarrier Studios

#include "Equipment/Data/EnclaveItemDefinition.h"

const FEnclaveItemActionBinding* UEnclaveItemDefinition::FindActionBinding(const FGameplayTag& InputTag) const
{
    if (!InputTag.IsValid())
    {
        return nullptr;
    }

    for (const FEnclaveItemActionBinding& Binding : ActionBindings)
    {
        if (Binding.InputTag.MatchesTagExact(InputTag))
        {
            return &Binding;
        }
    }

    return nullptr;
}
