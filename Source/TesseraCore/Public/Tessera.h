#pragma once

#include "CoreMinimal.h"

TESSERACORE_API DECLARE_LOG_CATEGORY_EXTERN(LogTessera, Log, All);

/**
 * Command-line switches, named with the game's prefix ([Tessera] CommandPrefix in DefaultGame.ini; default "TS"),
 * so each game keeps its own spelling: with CommandPrefix=SPY, TSCmd::Value(TEXT("Look"), Out) reads -SPYLook=hd2d.
 */
namespace TSCmd
{
	TESSERACORE_API const FString& Prefix();
	/** -<Prefix><Name> is present. */
	TESSERACORE_API bool Has(const TCHAR* Name);
	/** -<Prefix><Name>=<value>. bWhole keeps commas and spaces (lists such as "1,2,3"). */
	TESSERACORE_API bool Value(const TCHAR* Name, FString& Out, bool bWhole = false);
	TESSERACORE_API bool Value(const TCHAR* Name, float& Out);
	TESSERACORE_API bool Value(const TCHAR* Name, int32& Out);
}

/** Settings from the game's DefaultGame.ini, section [Tessera]. */
namespace TSConfig
{
	TESSERACORE_API FString Get(const TCHAR* Key, const TCHAR* Default);
}
