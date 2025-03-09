// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using UnrealBuildTool.Rules;

public class CommonInventory : ModuleRules
{
	public CommonInventory(ReadOnlyTargetRules Target) : base(Target)
	{
        IWYUSupport = IWYUSupport.Full;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        //Non-Unity build. https://forums.unrealengine.com/t/how-to-compile-in-non-unity-mode/94863/5
        //bUseUnity = false;

        PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "Engine",
                "VariadicStruct",
				"DeveloperSettings",
                "GameplayTags",
                "TraceLog",

            }
        );

        // StructUtils were migrated to CoreUObject in 5.5.0.
        if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion < 5)
        {
            PublicDependencyModuleNames.Add("StructUtils");
        }

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"NetCore",
                "AssetRegistry",

            }
        );
	}
}
