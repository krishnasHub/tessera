using UnrealBuildTool;

public class TesseraWorld : ModuleRules
{
	public TesseraWorld(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "TesseraCore", "ProceduralMeshComponent" });
		PrivateDependencyModuleNames.AddRange(new string[] { "NavigationSystem" });
	}
}
