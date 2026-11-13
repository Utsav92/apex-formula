using UnrealBuildTool;

public class APEXFormula : ModuleRules
{
	public APEXFormula(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Source is organised in Core/, Vehicles/, Racing/ ... so include paths are module-relative.
		PublicIncludePaths.Add(ModuleDirectory);
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
			"Slate", "SlateCore", "ApplicationCore", "AssetRegistry", "CinematicCamera", "Json"
		});
	}
}
