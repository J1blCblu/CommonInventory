// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CommonInventoryKismet : ModuleRules
{
	public CommonInventoryKismet(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
				"Engine",
                "CoreUObject",
                "CommonInventory",
				"BlueprintGraph",
            }
		);
	}
}
