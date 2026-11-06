// Copyright Lightcarrier Studios

#include "Game/EnclaveURLOptions.h"

#include "Kismet/GameplayStatics.h"

#include "Debug/EnclaveDebug.h"

bool EnclaveURLOptions::TryParseStrictInt(const FString& Value, int32& OutValue)
{
	const int32 FirstDigit = Value.StartsWith(TEXT("-")) ? 1 : 0;
	const int32 NumDigits = Value.Len() - FirstDigit;
	if (NumDigits < 1 || NumDigits > 9)
	{
		return false;
	}
	for (int32 Index = FirstDigit; Index < Value.Len(); ++Index)
	{
		if (!FChar::IsDigit(Value[Index]))
		{
			return false;
		}
	}
	OutValue = FCString::Atoi(*Value);
	return true;
}

bool EnclaveURLOptions::TryParseStrictFloat(const FString& Value, float& OutValue)
{
	const int32 First = Value.StartsWith(TEXT("-")) ? 1 : 0;
	if (Value.Len() - First < 1 || Value.Len() > 16)
	{
		return false;
	}

	int32 NumDigits = 0;
	int32 NumPoints = 0;
	for (int32 Index = First; Index < Value.Len(); ++Index)
	{
		if (FChar::IsDigit(Value[Index]))
		{
			++NumDigits;
		}
		else if (Value[Index] == TEXT('.') && ++NumPoints == 1)
		{
			continue;
		}
		else
		{
			return false;
		}
	}
	if (NumDigits == 0)
	{
		return false;
	}

	OutValue = FCString::Atof(*Value);
	return true;
}

void EnclaveURLOptions::ApplyInt(const FString& Options, const TCHAR* Key, int32 MinValue, int32& InOutValue)
{
	const FString Value = UGameplayStatics::ParseOption(Options, Key);
	if (Value.IsEmpty())
	{
		return;
	}

	int32 Parsed = 0;
	if (!TryParseStrictInt(Value, Parsed) || Parsed < MinValue)
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("Ignoring invalid %s server option '%s'. Expected an integer >= %d; keeping %d."),
			Key, *Value, MinValue, InOutValue);
		return;
	}

	InOutValue = Parsed;
}

void EnclaveURLOptions::ApplyFloat(const FString& Options, const TCHAR* Key, float MinValue, float MaxValue, float& InOutValue)
{
	const FString Value = UGameplayStatics::ParseOption(Options, Key);
	if (Value.IsEmpty())
	{
		return;
	}

	float Parsed = 0.0f;
	if (!TryParseStrictFloat(Value, Parsed) || Parsed < MinValue || Parsed > MaxValue)
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("Ignoring invalid %s server option '%s'. Expected a number from %g to %g; keeping %g."),
			Key, *Value, MinValue, MaxValue, InOutValue);
		return;
	}

	InOutValue = Parsed;
}

void EnclaveURLOptions::ApplyBool(const FString& Options, const TCHAR* Key, bool& InOutValue)
{
	const FString Value = UGameplayStatics::ParseOption(Options, Key);
	if (Value.IsEmpty())
	{
		return;
	}

	if (Value == TEXT("1") || Value.Equals(TEXT("true"), ESearchCase::IgnoreCase))
	{
		InOutValue = true;
	}
	else if (Value == TEXT("0") || Value.Equals(TEXT("false"), ESearchCase::IgnoreCase))
	{
		InOutValue = false;
	}
	else
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("Ignoring invalid %s server option '%s'. Expected 1 or 0; keeping %d."),
			Key, *Value, InOutValue ? 1 : 0);
	}
}

void EnclaveURLOptions::ApplyEnumValue(const FString& Options, const TCHAR* Key, const UEnum& Enum, int64& InOutValue)
{
	const FString Value = UGameplayStatics::ParseOption(Options, Key);
	if (Value.IsEmpty())
	{
		return;
	}

	// Accepts the short name, any case: the enum prefix is added when "::" is
	// missing (Enum.cpp:951-968). The generated _MAX entry isn't a real value.
	const int32 Index = Enum.GetIndexByNameString(Value);
	if (Index == INDEX_NONE || Enum.GetNameStringByIndex(Index).EndsWith(TEXT("_MAX")))
	{
		UE_LOG(LogEnclaveMatch, Warning, TEXT("Ignoring invalid %s server option '%s'; keeping %s."),
			Key, *Value, *Enum.GetNameStringByValue(InOutValue));
		return;
	}

	InOutValue = Enum.GetValueByIndex(Index);
}
