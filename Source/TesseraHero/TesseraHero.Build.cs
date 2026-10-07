using UnrealBuildTool;

public class TesseraHero : ModuleRules
{
	public TesseraHero(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Json", "TesseraCore", "TesseraGameplay" });
		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem" });
	}
}
