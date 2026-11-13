using UnrealBuildTool;

public class APEXFormulaTarget : TargetRules
{
	public APEXFormulaTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("APEXFormula");
	}
}
