// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryEditorUtility.h"
#include "CommonInventoryTypes.h"
#include "CommonItemCustomization.h"
#include "CommonItemDefinitionEditorStyle.h"
#include "InventoryRegistry/DataSources/AssetManagerDataSource.h"
#include "InventoryRegistry/CommonInventoryRegistry.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "Features/IModularFeatures.h"
#include "FileHelpers.h"
#include "IPIEAuthorizer.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/AssetRegistryTagsContext.h"

// InventoryRegistry cooking
#include "GameDelegates.h"
#include "Interfaces/ITargetPlatform.h"
#include "IPlatformFileSandboxWrapper.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Parse.h"

#if UE_VERSION_OLDER_THAN(5, 4, 0)
#include "CookOnTheSide/CookOnTheFlyServer.h"
#else
#include "UObject/ICookInfo.h"
#endif

#define LOCTEXT_NAMESPACE "CommonInventoryEditor"

static const FName AssetToolOwner = TEXT("CommonInventory");

static UAssetManagerDataSource* GetAssetManagerDataSource()
{
	return Cast<UAssetManagerDataSource>(UCommonInventoryRegistry::Get().GetDataSource());
}

class FCommonInventoryEditorModule final : public IModuleInterface, public IPIEAuthorizer
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual bool RequestPIEPermission(bool bIsSimulateInEditor, FString& OutReason) const override;

	void OnGatherObjectsForPropagation(const FCommonInventoryDefaultsPropagationContext& InContext, TArray<UObject*>& OutObjects, bool& bOutSkipGathering) const;

	// Registry Flushing
	void OnPreBeginPIE(const bool);
	void OnPreAssetValidation();

	// Registry Cooking
	void ModifyCook(TConstArrayView<const ITargetPlatform*> InTargetPlatforms, TArray<FName>&, TArray<FName>&);
	FString GetOutputDirectory() const;
	void OnCookFinished(class UE::Cook::ICookInfo& InCookInfo);
	void OnCookFinishedLegacy();

	// Registry Refreshing
	bool IsValidPrimaryAssetName(const UCommonItemDefinition* Definition) const;
	void OnInMemoryAssetCreated(UObject* Asset);
	void OnAssetRemoved(const FAssetData& AssetData);
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
	void OnEnginePreExit();
};

IMPLEMENT_MODULE(FCommonInventoryEditorModule, CommonInventoryEditor);

void FCommonInventoryEditorModule::StartupModule()
{
	if (IsRunningCookCommandlet())
	{
		// We're generating the cached binary during GenerateInitialRequests(), while the CookByTheBookStarted delegate fires too late.
		FGameDelegates::Get().GetModifyCookDelegate().AddRaw(this, &FCommonInventoryEditorModule::ModifyCook);

#if UE_VERSION_OLDER_THAN(5, 4, 0)
		UCookOnTheFlyServer::OnCookByTheBookFinished().AddRaw(this, &FCommonInventoryEditorModule::OnCookFinishedLegacy);
#else
		UE::Cook::FDelegates::CookByTheBookFinished.AddRaw(this, &FCommonInventoryEditorModule::OnCookFinished);
#endif
	}
	else if (!IsRunningCommandlet())
	{
		// Disallow the PIE session if the data source is waiting for an asynchronous response.
		IModularFeatures::Get().RegisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), this);

		// Propagate changes to dirty assets.
		FCommonInventoryDefaultsPropagator::Get().Bind_OnGatherObjectsOverride(FCommonInventoryDefaultsPropagator::FGatherObjectsOverrideDelegate::CreateRaw(this, &FCommonInventoryEditorModule::OnGatherObjectsForPropagation));

		// Flush UInventoryRegistry before PIE and data validation.
		FEditorDelegates::PreBeginPIE.AddRaw(this, &FCommonInventoryEditorModule::OnPreBeginPIE);
		FEditorDelegates::OnPreAssetValidation.AddRaw(this, &FCommonInventoryEditorModule::OnPreAssetValidation);

		// Refresh UAssetManagerDataSource.
		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		AssetRegistry.OnInMemoryAssetCreated().AddRaw(this, &FCommonInventoryEditorModule::OnInMemoryAssetCreated);
		AssetRegistry.OnAssetRemoved().AddRaw(this, &FCommonInventoryEditorModule::OnAssetRemoved);
		AssetRegistry.OnAssetRenamed().AddRaw(this, &FCommonInventoryEditorModule::OnAssetRenamed);
		FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCommonInventoryEditorModule::OnEnginePreExit);

		// Validate UCommonItemDefinition name to avoid ambiguity.
		IAssetTools::Get().RegisterIsNameAllowedDelegate(AssetToolOwner, FIsNameAllowed::CreateStatic(CommonInventoryEditor::IsNameAllowed));

		// Register custom asset category.
		IAssetTools::Get().RegisterAdvancedAssetCategory(AssetToolOwner, CommonInventoryEditor::GetAssetCategoryPath().GetCategoryText());

		// Details customization.
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(FCommonItem::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCommonItemCustomization::MakeInstance));
		PropertyModule.NotifyCustomizationModuleChanged();

		// Register custom styling.
		FCommonItemDefinitionEditorStyle::Get();
	}
}

void FCommonInventoryEditorModule::ShutdownModule()
{
	if (IsRunningCookCommandlet())
	{
		FGameDelegates::Get().GetModifyCookDelegate().RemoveAll(this);

#if UE_VERSION_OLDER_THAN(5, 4, 0)
		UCookOnTheFlyServer::OnCookByTheBookFinished().RemoveAll(this);
#else
		UE::Cook::FDelegates::CookByTheBookFinished.RemoveAll(this);
#endif
	}
	else if (!IsRunningCommandlet())
	{
		IModularFeatures::Get().UnregisterModularFeature(IPIEAuthorizer::GetModularFeatureName(), this);

		FCommonInventoryDefaultsPropagator::Get().Unbind_OnGatherObjectsOverride();

		FEditorDelegates::PreBeginPIE.RemoveAll(this);
		FEditorDelegates::OnPreAssetValidation.RemoveAll(this);
		FEditorDelegates::OnPreDestructiveAssetAction.RemoveAll(this);
		FCoreDelegates::OnEnginePreExit.RemoveAll(this);

		if (IAssetRegistry* const AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnInMemoryAssetCreated().RemoveAll(this);
			AssetRegistry->OnAssetRemoved().RemoveAll(this);
			AssetRegistry->OnAssetRenamed().RemoveAll(this);
		}

		if (FPropertyEditorModule* const PropertyModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyModule->UnregisterCustomPropertyTypeLayout("CommonItem");
			PropertyModule->NotifyCustomizationModuleChanged();
		}

		if (FAssetToolsModule* const AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
		{
			AssetToolsModule->Get().UnregisterIsNameAllowedDelegate(AssetToolOwner);
		}

		FCommonItemDefinitionEditorStyle::Shutdown();
	}
}

bool FCommonInventoryEditorModule::RequestPIEPermission(bool bIsSimulateInEditor, FString& OutReason) const
{
	// Ask DataSource if we can actually start PIE.
	if (const UCommonInventoryRegistryDataSource* const DataSource = UCommonInventoryRegistry::Get().GetDataSource())
	{
		return DataSource->RequestPIEPermission(OutReason, bIsSimulateInEditor);
	}

	return true;
}

void FCommonInventoryEditorModule::OnGatherObjectsForPropagation(const FCommonInventoryDefaultsPropagationContext& InContext, TArray<UObject*>& OutObjects, bool& bOutSkipGathering) const
{
	// We're just appending the list.
	bOutSkipGathering = false;
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

	// Gather dirty assets which have not yet serialized the searchable name.
	for (const UPackage* const Package : DirtyPackages)
	{
		GetObjectsWithOuter(
			Package,
			OutObjects,
			/* bIncludeNestedObjects */ true,
			/* ExclusionFlags */ RF_NoFlags,
			/* ExclusionInternalFlags */ EInternalObjectFlags::Garbage | EInternalObjectFlags::Async
		);
	}
}

void FCommonInventoryEditorModule::OnPreBeginPIE(const bool)
{
	UCommonInventoryRegistry::Get().FlushPendingRefresh();
}

void FCommonInventoryEditorModule::OnPreAssetValidation()
{
	UCommonInventoryRegistry::Get().FlushPendingRefresh();
}

void FCommonInventoryEditorModule::ModifyCook(TConstArrayView<const ITargetPlatform*> InTargetPlatforms, TArray<FName>&, TArray<FName>&)
{
	/**
	 * I haven't found a way to cook an in-memory package without saving and deleting it.
	 * There's a ICookPackageSplitter for generated packages, but it's intended for splitting up the package, such as UWorldPartition.
	 * DirectoriesToAlwaysStageAsUFS works with physical files and requires a 'game' config which isn't accessible from plugins.
	 * For now, we'll create InventoryRegistry.bin in the cook directory without creating artifacts in the project directories.
	 * Files in the root directory with non-special extensions (.uasset, .umap, .uexp, etc.) will be automatically caught up by UAT.
	 *
	 * Just some useful resources describing paths, packages, objects, etc.:
	 * https://dev.epicgames.com/community/learning/knowledge-base/D7nL/unreal-engine-primer-loading-content-and-pak-files-at-runtime
	 * https://heapcleaner.wordpress.com/2016/06/11/uobject-constructor-postinitproperties-and-postload/
	 * https://forums.unrealengine.com/t/save-load-package/372368
	 * https://forums.unrealengine.com/t/savepackage-does-not-create-uasset-on-disk/403692
	 * https://forums.unrealengine.com/t/creating-new-uasset-files-in-code-cpp/293942
	 */

	UCommonInventoryRegistry::Get().OnCookStarted();

	// AssetManagerEditorModule.cpp has an example in GetSavedAssetRegistryPath(TargetPlatform).
	const FString OutputDirectory = GetOutputDirectory();
	TUniquePtr<FSandboxPlatformFile> Sandbox = FSandboxPlatformFile::Create(false); // FCookSandbox in 5.4.
	Sandbox->Initialize(&FPlatformFileManager::Get().GetPlatformFile(), *FString::Printf(TEXT("-sandbox=\"%s\""), *OutputDirectory));

	const FString RegistryFilename = UCommonInventoryRegistry::GetRegistryFilename();
	const FString Filename = Sandbox->ConvertToAbsolutePathForExternalAppForWrite(*RegistryFilename);

	for (const ITargetPlatform* TargetPlatform : InTargetPlatforms)
	{
		if (ensure(TargetPlatform))
		{
			const FString PlatformFilename = Filename.Replace(TEXT("[Platform]"), *TargetPlatform->PlatformName());
			UCommonInventoryRegistry::Get().WriteForCook(TargetPlatform, PlatformFilename);
		}
	}
}

FString FCommonInventoryEditorModule::GetOutputDirectory() const
{
	//We could read -Sandbox= or -InjectedSandbox= path from the FCommandLine::GetSubprocessCommandLine(),
	//but UCookOnTheFlyServer doesn't initialize the sandbox with bEntireEngineWillUseThisSandbox set to true.
	FString OutputDirectory;
	FParse::Value(FCommandLine::Get(), TEXT("OutputDir="), OutputDirectory);
	if (OutputDirectory.IsEmpty())
	{
		OutputDirectory = FPaths::Combine(*FPaths::ProjectDir(), TEXT("Saved"), TEXT("Cooked"), TEXT("[Platform]"));
		OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
	}
	FPaths::NormalizeDirectoryName(OutputDirectory);
	return OutputDirectory;
}

void FCommonInventoryEditorModule::OnCookFinished(UE::Cook::ICookInfo& InCookInfo)
{
	UCommonInventoryRegistry::Get().OnCookFinished();
}

void FCommonInventoryEditorModule::OnCookFinishedLegacy()
{
	UCommonInventoryRegistry::Get().OnCookFinished();
}

bool FCommonInventoryEditorModule::IsValidPrimaryAssetName(const UCommonItemDefinition* Definition) const
{
	const FAssetData AssetData = FAssetData(Definition, FAssetData::ECreationFlags::SkipAssetRegistryTagsGathering);
	return CommonInventoryEditor::IsNameAllowed(Definition->GetName()) && CommonInventoryEditor::ValidateAssetName(AssetData);
}

void FCommonInventoryEditorModule::OnInMemoryAssetCreated(UObject* Asset)
{
	if (GIsPlayInEditorWorld) return;

	if (UCommonItemDefinition* const Definition = Cast<UCommonItemDefinition>(Asset))
	{
		if (IsValidPrimaryAssetName(Definition))
		{
			if (UAssetManagerDataSource* const DataSource = GetAssetManagerDataSource())
			{
				if (CommonInventoryEditor::CheckOutDefaultConfig(DataSource))
				{
					DataSource->OnAssetCreated(Definition);
				}
			}
		}
		else
		{
			const FText Text = FText::Format(LOCTEXT("DuplicateName", "Inventory Registry already contains an element named '{0}'. Please rename the asset."), FText::FromString(Definition->GetName()));
			const FText SubText = LOCTEXT("DuplicateNameDesc", "Duplicate names will cause issues with redirects.");
			CommonInventoryEditor::MakeNotification(Text, SubText, CommonInventoryEditor::FNotificationDelegate(), 15.f);
		}
	}
}

void FCommonInventoryEditorModule::OnAssetRemoved(const FAssetData& AssetData)
{
	if (GIsPlayInEditorWorld) return;

	if (UCommonInventoryRegistry::Get().ContainsRecord(AssetData.GetPrimaryAssetId()))
	{
		if (UAssetManagerDataSource* const DataSource = GetAssetManagerDataSource())
		{
			if (CommonInventoryEditor::CheckOutDefaultConfig(DataSource))
			{
				DataSource->OnAssetRemoved(AssetData);
			}
		}
	}
}

void FCommonInventoryEditorModule::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
	if (GIsPlayInEditorWorld) return;

	if (UCommonItemDefinition* const Definition = Cast<UCommonItemDefinition>(AssetData.GetAsset()))
	{
		const FPrimaryAssetId NewPrimaryAssetId = Definition->GetPrimaryAssetId();
		const FPrimaryAssetId OldPrimaryAssetId = Definition->SharedData.PrimaryAssetId;
		const FSoftObjectPath OldPath = OldObjectPath;

		if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().GetRegistryRecord(OldPrimaryAssetId); Record && Record->AssetPath == OldPath)
		{
			if (UAssetManagerDataSource* const DataSource = GetAssetManagerDataSource())
			{
				if (CommonInventoryEditor::CheckOutDefaultConfig(DataSource))
				{
					bool bRedirectorFailed = false;
					
					if (DataSource->OnAssetRenamed(Definition, OldPath, bRedirectorFailed); bRedirectorFailed)
					{
						CommonInventoryEditor::MakeNotification(
							FText::Format(LOCTEXT("RedirectionFailure", "InventoryRegistry: Failed to create a redirector from '{0}' to '{1}'."),
										  FText::FromName(OldPrimaryAssetId.PrimaryAssetName), FText::FromName(NewPrimaryAssetId.PrimaryAssetName)), true);
					}
				}
			}
		}
		else
		{
			// Try to register a new asset if the registry is unaware of it.
			OnInMemoryAssetCreated(Definition);
		}
	}
}

void FCommonInventoryEditorModule::OnEnginePreExit()
{
	TArray<UPackage*> DirtyPackages;
	FEditorFileUtils::GetDirtyContentPackages(DirtyPackages);

	// Cleanup stale redirects and config paths, and remove transient data from DevelopmentInventoryRegistry.bin.
	for (const UPackage* const Package : DirtyPackages)
	{
		// If the package exists only in memory (not saved).
		if (Package->HasAnyPackageFlags(PKG_NewlyCreated))
		{
			if (const UCommonItemDefinition* const Definition = Cast<UCommonItemDefinition>(Package->FindAssetInPackage()))
			{
				if (FAssetData AssetData; UAssetManager::Get().GetPrimaryAssetData(Definition->GetPrimaryAssetId(), AssetData))
				{
					OnAssetRemoved(AssetData);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE 
