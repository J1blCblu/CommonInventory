// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class CommonInventoryEditor : ModuleRules
{
	public CommonInventoryEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        IWYUSupport = IWYUSupport.Full;
		bUseUnity = false;

        //Useful editor utilities:
        // UEditorAssetLibrary - assets and directories lifetime.
        // IAssetTools - asset utilities (custom type actions, migration, import, diff, copy, duplicate/create/rename).
        // UPackageTools - packages. UEditorAssetLibrary is more useful in practice.
        // ObjectTools.h - lower level lib handling object/assets/packages. UEditorAssetLibrary is more useful in practice.
        // UEditorLoadingAndSavingUtils - saving and loading of packages and maps.
        // UEditorUtilityLibrary - selected objects polls.
        // UAssetEditorSubsystem - editor utils, like opening/closing/finding editor window for an asset.

        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "Engine",
                "UnrealEd",
                "CoreUObject",
                "VariadicStruct",
                "CommonInventory",
                
                //Details Customization
                "Slate",
                "SlateCore",
                "InputCore",
                "PropertyEditor",
                "ToolWidgets", // SSearchableComboBox

                //UCommonItemDefinition
                "AssetDefinition",
                "ClassViewer", //To display a group of items during definition creation.
                "ContentBrowser",
                "ContentBrowserData",
                "ToolMenus",
                "AssetTools",

				//InventoryRegistry
				"SandboxFile", //Access the cook directory.
				"TargetPlatform",

                //Settings
                "DeveloperSettings",
                "SourceControl",
            }
        );

        // StructUtils were migrated to CoreUObject in 5.5.0.
        if (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion < 5)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "StructUtils",
                    "StructUtilsEngine", // OnUserDefinedStructReinstanced delegate.
                    "StructUtilsEditor",
                });
        }
    }
}
