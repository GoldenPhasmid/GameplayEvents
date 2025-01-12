// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameplayEvents : ModuleRules
{
	public GameplayEvents(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[]
		{
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore"
		});

		PublicDefinitions.Add("REQUIRE_BASE_EVENT_CLASS = 0");
	}
}
