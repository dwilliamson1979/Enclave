// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveFocusViewInterface.generated.h"

class AActor;
class AEnclaveItem;
struct FEnclaveFocusViewSettings;

UINTERFACE(MinimalAPI)
class UEnclaveFocusViewInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Presentation-facing view contract. The caller has already resolved item
 * Focus settings; the view implementation owns only local camera response.
 * It never activates GAS abilities or handles item-action input.
 */
class ENCLAVE_API IEnclaveFocusViewInterface
{
	GENERATED_BODY()

public:
	virtual void BeginFocus(AEnclaveItem* SourceItem, const FEnclaveFocusViewSettings& Settings) = 0;
	/**
	 * A normal cue End blends out. When the item is being removed from the
	 * avatar's presentation, its old physical alignment must be discarded now,
	 * even though the camera FOV may continue blending out.
	 */
	virtual void EndFocus(AEnclaveItem* SourceItem, bool bSourcePresentationRemoved = false) = 0;

	/** Discover a view consumer without depending on a particular component. */
	static IEnclaveFocusViewInterface* FindFocusView(AActor* Avatar);
};
