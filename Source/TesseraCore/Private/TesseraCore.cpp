#include "Tessera.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, TesseraCore);

DEFINE_LOG_CATEGORY(LogTessera);

FString TSConfig::Get(const TCHAR* Key, const TCHAR* Default)
{
	FString V;
	return GConfig && GConfig->GetString(TEXT("Tessera"), Key, V, GGameIni) && !V.IsEmpty() ? V : FString(Default);
}

const FString& TSCmd::Prefix()
{
	static const FString P = TSConfig::Get(TEXT("CommandPrefix"), TEXT("TS"));
	return P;
}

bool TSCmd::Has(const TCHAR* Name) { return FParse::Param(FCommandLine::Get(), *(Prefix() + Name)); }

bool TSCmd::Value(const TCHAR* Name, FString& Out, bool bWhole)
{
	return FParse::Value(FCommandLine::Get(), *(Prefix() + Name + TEXT("=")), Out, !bWhole);
}

bool TSCmd::Value(const TCHAR* Name, float& Out) { return FParse::Value(FCommandLine::Get(), *(Prefix() + Name + TEXT("=")), Out); }
bool TSCmd::Value(const TCHAR* Name, int32& Out) { return FParse::Value(FCommandLine::Get(), *(Prefix() + Name + TEXT("=")), Out); }
