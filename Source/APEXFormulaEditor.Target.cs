using UnrealBuildTool;

public class APEXFormulaEditorTarget : TargetRules
{
	public APEXFormulaEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("APEXFormula");
	}
}
