// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveItemAttachmentDefinition.generated.h"

/**
 * Defines the skeletal-mesh sockets used to present a particular item type
 * on an avatar.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemAttachmentMapping
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName WieldedSocket;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName StowedSocket;
};

/**
 * Defines how item classifications map to attachment sockets for an avatar.
 *
 * The item identifies what it is. The avatar selects an attachment definition
 * that determines how items of that type are physically presented.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveItemAttachmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FEnclaveItemAttachmentMapping* FindAttachmentMapping(const FGameplayTag& ItemType) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment", meta = (AllowPrivateAccess = "true"))
	TMap<FGameplayTag, FEnclaveItemAttachmentMapping> ItemAttachmentMappings;
};