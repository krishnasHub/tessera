using UnrealBuildTool;

public class TesseraUI : ModuleRules
{
	public TesseraUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "Json", "TesseraCore", "TesseraGameplay" });
		PrivateDependencyModuleNames.AddRange(new string[] { "RenderCore", "TesseraWorld" });
	}
}
