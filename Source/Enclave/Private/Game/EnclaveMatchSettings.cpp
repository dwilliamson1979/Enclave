// Copyright Lightcarrier Studios

#include "Game/EnclaveMatchSettings.h"

#include "Kismet/GameplayStatics.h"

#include "Debug/EnclaveDebug.h"

namespace
{
	/**
	 * Strict: digits only, with an optional leading '-'. LexTryParseString and
	 * GetIntOption both accept "2.5" as 2 and "3abc" as 3. Nine digits at most,
	 * so the value always fits in an int32.
	 */
	bool TryParseStrictInt(const FString& Value, int32& OutValue)
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

	/**
	 * Reads ?Key=Value as an integer >= MinValue. A missing key keeps the current
	 * value. A non-integer or out-of-range value is logged and also keeps it.
	 */
	void ApplyIntOption(const FString& Options, const TCHAR* Key, int32 MinValue, int32& InOutValue)
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
}

void FEnclaveMatchSettings::ApplyOptions(const FString& Options)
{
	ApplyIntOption(Options, TEXT("ScoreLimit"), 0, ScoreLimit);
	ApplyIntOption(Options, TEXT("TimeLimit"), 0, TimeLimitSeconds);
	ApplyIntOption(Options, TEXT("RespawnDelay"), 0, RespawnDelaySeconds);
	ApplyIntOption(Options, TEXT("Warmup"), 0, WarmupSeconds);
	ApplyIntOption(Options, TEXT("MinPlayers"), 1, MinPlayersToStart);
	ApplyIntOption(Options, TEXT("PostMatch"), 0, PostMatchSeconds);
}

FString FEnclaveMatchSettings::ToString() const
{
	return FString::Printf(TEXT("ScoreLimit=%d TimeLimit=%ds RespawnDelay=%ds Warmup=%ds MinPlayers=%d PostMatch=%ds"),
		ScoreLimit, TimeLimitSeconds, RespawnDelaySeconds, WarmupSeconds, MinPlayersToStart, PostMatchSeconds);
}
