// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CommonInventoryTests : ModuleRules
{
	public CommonInventoryTests(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "CommonInventory",

            }
		);
	}
}
