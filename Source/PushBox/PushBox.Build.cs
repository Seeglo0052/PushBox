// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PushBox : ModuleRules
{
	public PushBox(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Public",
			ModuleDirectory + "/Public/Data",
			ModuleDirectory + "/Public/Grid",
			ModuleDirectory + "/Public/Actors",
			ModuleDirectory + "/Public/Core",
			ModuleDirectory + "/Public/UI",
			ModuleDirectory + "/Public/Utilities",
		});

		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Private",
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayTags",
			"UMG",
			"CommonInput",
			"CommonUI",

			// Plugins
			"WFC",
			"GameplayMessageRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"Slate",
			"SlateCore",
		});
	}
}
