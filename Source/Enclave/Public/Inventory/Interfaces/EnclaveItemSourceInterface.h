// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EnclaveItemSourceInterface.generated.h"

class AEnclaveItem;

DECLARE_MULTICAST_DELEGATE(FOnEnclaveItemSourceChanged);

UINTERFACE(MinimalAPI)
class UEnclaveItemSourceInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for anything that can supply a list of items to a consumer such
 * as EquipmentComponent, without exposing which concrete storage owns them.
 *
 * Implementers are responsible for broadcasting OnItemSourceChanged whenever
 * the set of items they expose changes (added, removed, or replicated in).
 */
class ENCLAVE_API IEnclaveItemSourceInterface
{
	GENERATED_BODY()

public:
	virtual const TArray<TObjectPtr<AEnclaveItem>>& GetSourceItems() const = 0;
	virtual bool ContainsSourceItem(const AEnclaveItem* Item) const = 0;
	virtual FOnEnclaveItemSourceChanged& OnItemSourceChanged() = 0;
};
