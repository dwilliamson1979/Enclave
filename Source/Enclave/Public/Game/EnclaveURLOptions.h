// Copyright Lightcarrier Studios

#pragma once

#include "CoreMinimal.h"

/**
 * Server URL options (MapName?Key=Value?...) applied on top of a setting's
 * current value. One convention for every option: a missing key keeps the
 * value; an invalid value is logged (LogEnclaveMatch) and also keeps it.
 * Operator input never ensures or fails the server.
 *
 * Numbers are parsed strictly, because the engine helpers accept "2.5" as 2
 * and "3abc" as 3 (LexTryParseString, GetIntOption).
 */
namespace EnclaveURLOptions
{
	/** ?Key=<integer >= MinValue>. */
	ENCLAVE_API void ApplyInt(const FString& Options, const TCHAR* Key, int32 MinValue, int32& InOutValue);

	/** ?Key=<number in [MinValue, MaxValue]>, e.g. 0.5. */
	ENCLAVE_API void ApplyFloat(const FString& Options, const TCHAR* Key, float MinValue, float MaxValue, float& InOutValue);

	/** ?Key=1|0|true|false (any case). */
	ENCLAVE_API void ApplyBool(const FString& Options, const TCHAR* Key, bool& InOutValue);

	/** ?Key=<enum entry name>, short and any case ("random"). Writes the value of the entry found. */
	ENCLAVE_API void ApplyEnumValue(const FString& Options, const TCHAR* Key, const UEnum& Enum, int64& InOutValue);

	/** ApplyEnumValue for a UENUM type. */
	template<typename TEnum>
	void ApplyEnum(const FString& Options, const TCHAR* Key, TEnum& InOutValue)
	{
		int64 Value = static_cast<int64>(InOutValue);
		ApplyEnumValue(Options, Key, *StaticEnum<TEnum>(), Value);
		InOutValue = static_cast<TEnum>(Value);
	}

	/** Strict integer: digits only, an optional leading '-', at most nine digits (always fits in an int32). */
	ENCLAVE_API bool TryParseStrictInt(const FString& Value, int32& OutValue);

	/** Strict number: digits with at most one '.', an optional leading '-', at least one digit. */
	ENCLAVE_API bool TryParseStrictFloat(const FString& Value, float& OutValue);
}
