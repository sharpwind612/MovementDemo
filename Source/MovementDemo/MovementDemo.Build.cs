// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovementDemo : ModuleRules
{
	public MovementDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });
	}
}
