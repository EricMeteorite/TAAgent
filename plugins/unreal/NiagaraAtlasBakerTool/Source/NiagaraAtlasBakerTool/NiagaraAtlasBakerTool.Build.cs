using UnrealBuildTool;

public class NiagaraAtlasBakerTool : ModuleRules
{
    public NiagaraAtlasBakerTool(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine"
            });

        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "AdvancedPreviewScene",
                "AssetRegistry",
                "Blutility",
                "EditorFramework",
                "EditorScriptingUtilities",
                "ImageWrapper",
                "InputCore",
                "LevelEditor",
                "Niagara",
                "NiagaraEditor",
                "Projects",
                "PropertyEditor",
                "RenderCore",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd"
            });
    }
}
