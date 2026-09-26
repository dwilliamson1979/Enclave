// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "EnclaveItemPresentationDefinition.generated.h"

/**
 * How one avatar presents one item type: where it attaches (sockets) and -
 * as the avatar-animation work lands - which animation layers and wielder
 * montages it uses. Everything in a row is specific to the avatar's
 * skeleton, which is why it lives here and not on the item.
 */
USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemPresentationMapping
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName FirstPersonWieldedSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName ThirdPersonWieldedSocket = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attachment")
	FName StowedSocket = NAME_None;
};

/**
 * Defines how item classifications map to presentation data for an avatar.
 *
 * The item identifies what it is. The avatar selects a presentation definition
 * that determines how items of that type are physically presented.
 *
 * One asset per avatar skeleton/body plan (e.g. DA_ItemPresentation_Humanoid).
 * Renamed from UEnclaveItemAttachmentDefinition 2026-09-26 once it grew
 * beyond sockets - see the temporary [CoreRedirects] in DefaultEngine.ini.
 */
UCLASS(BlueprintType)
class ENCLAVE_API UEnclaveItemPresentationDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FEnclaveItemPresentationMapping* FindPresentationMapping(const FGameplayTag& ItemType) const;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presentation", meta = (AllowPrivateAccess = "true"))
	TMap<FGameplayTag, FEnclaveItemPresentationMapping> ItemPresentationMappings;
};