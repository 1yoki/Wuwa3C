// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Wuwa : ModuleRules
{
	public Wuwa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EnhancedInput",
				"InputCore",
				"NetCore",
				"Niagara",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"Slate",
				"SlateCore",
				"UMG",
			});

		PublicIncludePaths.Add("Wuwa");
	}
}
