using UnrealBuildTool;
public class GarmentFitClothProbeHostEditorTarget : TargetRules
{
    public GarmentFitClothProbeHostEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("GarmentFitClothProbeHost");
    }
}
