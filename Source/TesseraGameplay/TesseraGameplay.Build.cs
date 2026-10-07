using UnrealBuildTool;

public class TesseraGameplay : ModuleRules
{
	public TesseraGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "TesseraCore" });
		PrivateDependencyModuleNames.AddRange(new string[] { "Niagara", "TesseraWorld", "ProceduralMeshComponent" });
	}
}
