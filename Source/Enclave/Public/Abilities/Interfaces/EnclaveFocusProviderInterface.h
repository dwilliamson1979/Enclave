// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveFocusProviderInterface.generated.h"

class UEnclaveFocusProfile;

UINTERFACE(MinimalAPI)
class UEnclaveFocusProviderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * An item capability: resolves the currently effective view profile for Focus.
 * A firearm returns its definition's profile today; an installed optic can
 * override this resolution later without altering the GAS ability or avatar.
 */
class ENCLAVE_API IEnclaveFocusProviderInterface
{
	GENERATED_BODY()

public:
	virtual const UEnclaveFocusProfile* ResolveFocusProfile() const = 0;
};
