// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Wuwa : ModuleRules
{
	public Wuwa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate",
			"GameplayTags",
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"Wuwa",
			"Wuwa/Variant_Platforming",
			"Wuwa/Variant_Platforming/Animation",
			"Wuwa/Variant_Combat",
			"Wuwa/Variant_Combat/AI",
			"Wuwa/Variant_Combat/Animation",
			"Wuwa/Variant_Combat/Gameplay",
			"Wuwa/Variant_Combat/Interfaces",
			"Wuwa/Variant_Combat/UI",
			"Wuwa/Variant_SideScrolling",
			"Wuwa/Variant_SideScrolling/AI",
			"Wuwa/Variant_SideScrolling/Gameplay",
			"Wuwa/Variant_SideScrolling/Interfaces",
			"Wuwa/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
