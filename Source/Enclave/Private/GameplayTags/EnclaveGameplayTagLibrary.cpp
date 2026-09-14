// Copyright Lightcarrier Studios

#include "GameplayTags/EnclaveGameplayTagLibrary.h"

FName UEnclaveGameplayTagLibrary::GetLeafName(const FGameplayTag& GameplayTag)
{
	if (!GameplayTag.IsValid())
	{
		return NAME_None;
	}

	const FString TagString = GameplayTag.ToString();

	int32 LastDotIndex = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), LastDotIndex))
	{
		return FName(*TagString.RightChop(LastDotIndex + 1));
	}

	return FName(*TagString);
}