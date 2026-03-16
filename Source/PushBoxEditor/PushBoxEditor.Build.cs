// Copyright PushBox Game. All Rights Reserved.

using UnrealBuildTool;

public class PushBoxEditor : ModuleRules
{
	public PushBoxEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Public",
		});

		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Private",
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",

			// Our runtime module
			"PushBox",

			// WFC
			"WFC",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
			"InputCore",
			"UnrealEd",
			"EditorStyle",
			"ToolMenus",
			"EditorScriptingUtilities",
			"EditorSubsystem",
			"PropertyEditor",
			"Projects",
			"ContentBrowser",
			"AssetTools",
			"LevelEditor",
			"WorkspaceMenuStructure",
		});
	}
}
