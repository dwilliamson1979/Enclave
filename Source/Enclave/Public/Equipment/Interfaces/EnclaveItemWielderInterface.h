// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "EnclaveItemWielderInterface.generated.h"

class USceneComponent;

USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveItemAttachmentTarget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attachment")
	TObjectPtr<USceneComponent> ParentComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Attachment")
	FName SocketName = NAME_None;
};

USTRUCT(BlueprintType)
struct ENCLAVE_API FEnclaveWieldedItemAttachmentTargets
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attachment")
	FEnclaveItemAttachmentTarget FirstPerson;

	UPROPERTY(BlueprintReadOnly, Category = "Attachment")
	FEnclaveItemAttachmentTarget ThirdPerson;
};

UINTERFACE(MinimalAPI)
class UEnclaveItemWielderInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract for actors that can physically wield Enclave items.
 *
 * The wielder owns the rules that determine where a particular item type
 * should be presented. Consumers receive resolved attachment targets rather
 * than depending on the wielder's internal meshes or attachment definitions.
 */
class ENCLAVE_API IEnclaveItemWielderInterface
{
	GENERATED_BODY()

public:
	virtual bool ResolveWieldedItemAttachmentTargets(const FGameplayTag& ItemType, FEnclaveWieldedItemAttachmentTargets& OutTargets) const = 0;
};