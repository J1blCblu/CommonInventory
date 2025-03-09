// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "InventoryRegistry/DataSources/AssetManagerDataSource.h"

#include "InventoryRegistry/CommonInventoryRegistryBridge.h"
#include "CommonInventoryLog.h"
#include "CommonInventoryTrace.h"

#include "CoreGlobals.h"
#include "Delegates/Delegate.h"
#include "Engine/AssetManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Templates/UnrealTemplate.h"

#if UE_VERSION_OLDER_THAN(5, 4, 0)
#include "AssetRegistry/IAssetRegistry.h"
#endif // UE_VERSION_OLDER_THAN(5, 4, 0)

#if WITH_EDITOR
#include "Algo/Find.h"
#include "AssetRegistry/AssetData.h"
#include "CommonInventorySettings.h"
#include "Engine/AssetManagerSettings.h"
#include "Misc/PackageName.h"
#include "Misc/RedirectCollector.h"
#endif // WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetManagerDataSource)

UAssetManagerDataSource::UAssetManagerDataSource()
	: bKeepLoadedAssets(false)
	, bIsPendingRefresh(false)
	, bIsPendingCook(false)
	, bIsRefreshing(false)
{
	Traits.bIsPersistent = true;
	Traits.bSupportsCooking = true;
	Traits.bSupportsDevelopmentCooking = true;
}

void UAssetManagerDataSource::Initialize(ICommonInventoryRegistryBridge* InRegistryBridge)
{
	Super::Initialize(InRegistryBridge);

#if UE_VERSION_OLDER_THAN(5, 4, 0)
	// AssetManager has a race condition primarily in a non-editor asynchronous environment.
	// During the initial scan it relies on AssetRegistry.bin to be loaded, but there's no such guarantees.
	// We can use a synchronous scan to force the asset registry to load the premade.
	IAssetRegistry::GetChecked().ScanPathsSynchronous({}, /* bForceRescan */ false, /* bIgnoreDenyListScanFilters */ false);
#endif
}

void UAssetManagerDataSource::PostInitialize()
{
	RefreshAssetManagerTypeInfoList();

	if (!RegistryBridge->WasLoaded())
	{
		ForceRefresh(/* bSynchronous */ true);
	}
#if WITH_EDITOR
	else
	{
		// Force refresh editor data after the initial scan has completed to sync up outdated data (.uasset files could be removed, etc.).
		UAssetManager::Get().CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateUObject(this, &ThisClass::ForceRefresh, false));
	}

	// Listen for type info updates from UAssetManagerSettings.
	GetMutableDefault<UAssetManagerSettings>()->OnSettingChanged().AddWeakLambda(this, [this](UObject*, FPropertyChangedEvent&)
	{
		// Force ReinitializeFromConfig() before PreloadPrimaryAssets().
		UAssetManager::Get().ReinitializeFromConfig();
		ForceRefresh(false);
	});
#endif

	// Block further updates during the game.
	Super::PostInitialize();
}

void UAssetManagerDataSource::Deinitialize()
{
#if WITH_EDITOR
	GetMutableDefault<UAssetManagerSettings>()->OnSettingChanged().RemoveAll(this);
#endif
	CancelPendingRefresh();
	Super::Deinitialize();
}

void UAssetManagerDataSource::ForceRefresh(bool bSynchronous /* = false */)
{
	if (CanRefreshRegistry())
	{
		if (!IsPendingRefresh())
		{
			PreloadHandle = PreloadPrimaryAssets();

			if (PreloadHandle && PreloadHandle->IsActive())
			{
				bIsPendingRefresh = true;
			}
		}

		if (bSynchronous)
		{
			FlushPendingRefresh();
		}
	}
}

void UAssetManagerDataSource::FlushPendingRefresh()
{
	if (IsPendingRefresh())
	{
		if (PreloadHandle->IsLoadingInProgress())
		{
			// Unbind a complete delegate in case GStreamableDelegateDelayFrames > 0.
			PreloadHandle->BindCompleteDelegate(FStreamableDelegate());
		}

		if (PreloadHandle->WaitUntilComplete() == EAsyncPackageState::Complete)
		{
			BulkRefresh();
		}
	}
}

void UAssetManagerDataSource::CancelPendingRefresh()
{
	if (IsPendingRefresh())
	{
		PreloadHandle->CancelHandle();
		bIsPendingRefresh = false;
	}
}

bool UAssetManagerDataSource::IsPendingRefresh() const
{
	return bIsPendingRefresh && ensure(PreloadHandle && PreloadHandle->IsActive());
}

bool UAssetManagerDataSource::IsRefreshing() const
{
	return bIsRefreshing;
}

bool UAssetManagerDataSource::CanRefreshRegistry() const
{
#if WITH_EDITOR
	return UAssetManager::IsInitialized() && (!IsInitialized() || !GIsPlayInEditorWorld) && RegistryBridge && !RegistryBridge->IsCooking();
#else
	return UAssetManager::IsInitialized() && !IsInitialized();
#endif
}

void UAssetManagerDataSource::RefreshAssetManagerTypeInfoList()
{
#if UE_VERSION_OLDER_THAN(5, 4, 0)
	ensure(ScanTypeInfoList.IsEmpty(), TEXT("UAssetManagerDataSource::ScanTypeInfoList is only supported since 5.4.0. Use the AssetManager settings instead."));
#else
	// The easiest way to reset previously registered type infos.
	for (const FPrimaryAssetType PrimaryAssetType : RegisteredTypeInfoList)
	{
		UAssetManager::Get().RemovePrimaryAssetType(PrimaryAssetType);
	}

	RegisteredTypeInfoList.Reset();

	for (const FCommonInventoryTypeInfo& TypeInfo : ScanTypeInfoList)
	{
		FPrimaryAssetTypeInfo AssetTypeInfo;
		AssetTypeInfo.PrimaryAssetType = TypeInfo.PrimaryAssetType;
		AssetTypeInfo.SetAssetBaseClass(TypeInfo.AssetBaseClass);
		AssetTypeInfo.bIsEditorOnly = TypeInfo.bIsEditorOnly;
		AssetTypeInfo.GetDirectories() = TypeInfo.Directories;
		AssetTypeInfo.GetSpecificAssets() = TypeInfo.SpecificAssets;

		// We're not registering rules here since they are reset on each UAssetManager::ReinitializeFromConfig().
		// UAssetManagerSettings::CustomPrimaryAssetRules or primary asset labels can be utilized instead.
		if (UAssetManager::Get().ShouldScanPrimaryAssetType(AssetTypeInfo))
		{
			RegisteredTypeInfoList.Emplace(AssetTypeInfo.PrimaryAssetType);

			UAssetManager::Get().ScanPathsForPrimaryAssets(
				AssetTypeInfo.PrimaryAssetType,
				AssetTypeInfo.AssetScanPaths,
				AssetTypeInfo.AssetBaseClassLoaded,
				/* bHasBlueprintClasses */ false,
				AssetTypeInfo.bIsEditorOnly,
				/* bForceSynchronousScan */ false);
		}
	}
#endif
}

TSharedPtr<FStreamableHandle> UAssetManagerDataSource::PreloadPrimaryAssets()
{
	COMMON_INVENTORY_SCOPED_TRACE(UAssetManagerDataSource::PreloadPrimaryAssets);

	UAssetManager& AssetManager = UAssetManager::Get();
	TArray<FPrimaryAssetTypeInfo> TypeInfoList;
	TypeInfoList.Reserve(24);

	// Conditionally refresh the manager.
	AssetManager.RefreshPrimaryAssetDirectory(/* bForceRefresh */ false);
	AssetManager.GetPrimaryAssetTypeInfoList(TypeInfoList);

	// Exclude irrelevant types.
	TypeInfoList.RemoveAllSwap([this](const FPrimaryAssetTypeInfo& TypeInfo)
	{
		check(TypeInfo.AssetBaseClassLoaded);

		// Dynamic asset types aren't supported.
		if (TypeInfo.bIsDynamicAsset)
		{
			return true;
		}

		// Blueprint Classes, aka assets with data inheritance, aren't supported.
		if (TypeInfo.bHasBlueprintClasses)
		{
			return true;
		}

#if WITH_EDITOR

		// Skip editor only types if cooking.
		if (bIsPendingCook && TypeInfo.bIsEditorOnly)
		{
			return  true;
		}

#endif // WITH_EDITOR

		// We aren't bound to a single primary asset type, so check the base class instead.
		return !TypeInfo.AssetBaseClassLoaded->IsChildOf(UCommonItemDefinition::StaticClass());
	}, EAllowShrinking::No);

	ScanPrimaryAssetPaths(TypeInfoList);
	TArray<FPrimaryAssetId> FoundPrimaryAssets;
	FoundPrimaryAssets.Reserve(64 * TypeInfoList.Num());

	for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfoList)
	{
		AssetManager.GetPrimaryAssetIdList(TypeInfo.PrimaryAssetType, FoundPrimaryAssets);
	}

#if WITH_EDITOR

	// Filter assets based on cook rules.
	if (bIsPendingCook)
	{
		FoundPrimaryAssets.RemoveAllSwap([&AssetManager](const FPrimaryAssetId& InPrimaryAssetId)
		{
			const FName PackageName = AssetManager.GetPrimaryAssetPath(InPrimaryAssetId).GetLongPackageFName();
			return !AssetManager.VerifyCanCookPackage(nullptr, PackageName, /* bLogError */ false);
		}, EAllowShrinking::No);
	}

#endif // WITH_EDITOR

	TSharedPtr<FStreamableHandle> StreamableHandle;

	if (!FoundPrimaryAssets.IsEmpty())
	{
		StreamableHandle = AssetManager.PreloadPrimaryAssets(
			FoundPrimaryAssets,
			/* LoadBundles */ TArray<FName>(),
			/* bLoadRecursive */ false,
			FStreamableDelegate::CreateUObject(this, &ThisClass::BulkRefresh),
			FStreamableManager::AsyncLoadHighPriority);
	}

	return StreamableHandle;
}

void UAssetManagerDataSource::BulkRefresh()
{
	COMMON_INVENTORY_SCOPED_TRACE(UAssetManagerDataSource::BulkRefresh);

	if (!IsRefreshing() && IsPendingRefresh() && PreloadHandle->HasLoadCompleted())
	{
		const TGuardValue RefreshingGuard(bIsRefreshing, true);

		int32 LoadedNum, RequestedNum;
		PreloadHandle->GetLoadedCount(LoadedNum, RequestedNum);

		TArray<FCommonInventoryRegistryRecord, TInlineAllocator<256>> RegistryRecords;
		RegistryRecords.Reserve(LoadedNum);

		// Populate registry records.
		PreloadHandle->ForEachLoadedAsset([&RegistryRecords](UObject* LoadedAsset)
		{
			// This will fail with data only blueprint assets.
			if (UCommonItemDefinition* const Definition = CastChecked<UCommonItemDefinition>(LoadedAsset))
			{
				RegistryRecords.Emplace(*Definition);
			}
		});

		// Finally, update the registry.
		if (ensure(RegistryBridge))
		{
			RegistryBridge->ResetRecords(RegistryRecords);
		}

		// Check if we should keep loaded assets.
		if (!bKeepLoadedAssets)
		{
			PreloadHandle->ReleaseHandle();
		}

		bIsPendingRefresh = false;
	}
}

void UAssetManagerDataSource::ScanPrimaryAssetPaths(TArrayView<FPrimaryAssetTypeInfo> TypeInfoList)
{
	// We will slightly stall the editor startup by scanning the necessary types.
	if (UAssetManager& AssetManager = UAssetManager::Get(); !AssetManager.HasInitialScanCompleted())
	{
		AssetManager.PushBulkScanning();

		for (const FPrimaryAssetTypeInfo& TypeInfo : TypeInfoList)
		{
			AssetManager.ScanPathsForPrimaryAssets(
				TypeInfo.PrimaryAssetType,
				TypeInfo.AssetScanPaths,
				TypeInfo.AssetBaseClassLoaded,
				TypeInfo.bHasBlueprintClasses,
				TypeInfo.bIsEditorOnly,
				/* bForceSynchronousScan */ true);
		}

		AssetManager.PopBulkScanning();
	}
}

#if WITH_EDITOR

void UAssetManagerDataSource::AppendTypeInfoList(FCommonInventoryTypeInfo& InTypeInfo, const UCommonItemDefinition* InItemDefinition)
{
	const FString PackagePath = FPackageName::GetLongPackagePath(InItemDefinition->GetPathName());

	// Do nothing if it is already in any directory.
	if (InTypeInfo.Directories.ContainsByPredicate([&PackagePath](const FDirectoryPath& Directory) { return PackagePath.StartsWith(Directory.Path); }))
	{
		return;
	}

	// Num assets to convert from SpecificAssets to Directories.
	constexpr int32 AssetPathsConversionThreshold = 3;
	int32 AssetPathsInDirectory = 1; // Including the added one.

	for (const FSoftObjectPath& SpecificAsset : InTypeInfo.SpecificAssets)
	{
		if (PackagePath == FPackageName::GetLongPackagePath(SpecificAsset.GetAssetPathString()))
		{
			if (++AssetPathsInDirectory > AssetPathsConversionThreshold)
			{
				break;
			}
		}
	}

	Modify(/* bAlwaysMarkDirty */ false);

	if (AssetPathsInDirectory > AssetPathsConversionThreshold)
	{
		// Remove all subdirectories with paths and add a new directory.
		InTypeInfo.Directories.RemoveAll([&PackagePath](const FDirectoryPath& Directory) { return Directory.Path.StartsWith(PackagePath); });
		InTypeInfo.SpecificAssets.RemoveAll([&PackagePath](const FSoftObjectPath& Path) { return FPackageName::GetLongPackagePath(Path.GetAssetPathString()).StartsWith(PackagePath); });
		InTypeInfo.Directories.Emplace(PackagePath);
	}
	else
	{
		FSoftObjectPath OriginalPath = FSoftObjectPath(InItemDefinition);

		// GRedirectCollector keeps redirects during a session after deleting an asset, at least for FSoftObjectPath.
		// So once the original asset is recreated and its path is added to the config, the config will retain the old redirected value.
		if (!GRedirectCollector.GetAssetPathRedirection(OriginalPath).IsNull())
		{
			GRedirectCollector.RemoveAssetPathRedirection(OriginalPath);
		}

		InTypeInfo.SpecificAssets.Emplace(MoveTemp(OriginalPath));
	}

	TryUpdateDefaultConfigFile();
	RefreshAssetManagerTypeInfoList();
}

void UAssetManagerDataSource::OnAssetCreated(UCommonItemDefinition* InItemDefinition)
{
	check(RegistryBridge);

	if (InItemDefinition && CanRefreshRegistry())
	{
		bool bCanAppend = true;

		// Refresh the asset manager first due to inconsistent (formally) delegate broadcast order.
		UAssetManager::Get().RefreshAssetData(InItemDefinition);

		// Update our type info list if needed.
		if (!UAssetManager::Get().GetPrimaryAssetIdForObject(InItemDefinition).IsValid())
		{
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
			const FPrimaryAssetType PrimaryAssetType = InItemDefinition->GetPrimaryAssetId().PrimaryAssetType;

			// Append an existing type info.
			if (FCommonInventoryTypeInfo* const TypeInfo = Algo::FindBy(ScanTypeInfoList, PrimaryAssetType, &FCommonInventoryTypeInfo::PrimaryAssetType))
			{
				AppendTypeInfoList(*TypeInfo, InItemDefinition);
			}
			else if (Algo::FindBy(GetDefault<UAssetManagerSettings>()->PrimaryAssetTypesToScan, PrimaryAssetType, &FPrimaryAssetTypeInfo::PrimaryAssetType) == nullptr)
			{
				Modify(/* bAlwaysMarkDirty */ false);

				// Create a new type info.
				ScanTypeInfoList.Emplace(
					PrimaryAssetType,
					InItemDefinition->GetClass(),
					/* bIsEditorOnly */ false,
					TArray<FDirectoryPath>(),
					TArray<FSoftObjectPath>({ InItemDefinition->GetPathName() })
				);

				TryUpdateDefaultConfigFile();
				RefreshAssetManagerTypeInfoList();
			}
			else
#endif // !UE_VERSION_OLDER_THAN(5, 4, 0)
			{
				bCanAppend = false;
			}
		}

		if (bCanAppend)
		{
			RegistryBridge->AppendRecords({ FCommonInventoryRegistryRecord(*InItemDefinition) });
		}
	}
}

void UAssetManagerDataSource::OnAssetRemoved(const FAssetData& InRemovedAsset)
{
	check(RegistryBridge);

	if (InRemovedAsset.IsValid() && CanRefreshRegistry())
	{
		const FPrimaryAssetId PrimaryAssetId = InRemovedAsset.GetPrimaryAssetId();

		if (const FCommonInventoryRegistryRecord* const Record = RegistryBridge->GetRegistryRecord(PrimaryAssetId))
		{
			const FSoftObjectPath AssetPath = InRemovedAsset.GetSoftObjectPath();

			// Otherwise, duplicates will result in the removal of the original data.
			if (Record->AssetPath == AssetPath)
			{
#if !UE_VERSION_OLDER_THAN(5, 4, 0)

				// TypeInfo might be set directly in the asset manager settings.
				if (FCommonInventoryTypeInfo* const TypeInfo = Algo::FindBy(ScanTypeInfoList, PrimaryAssetId.PrimaryAssetType, &FCommonInventoryTypeInfo::PrimaryAssetType))
				{
					Modify(/* bAlwaysMarkDirty */ false);

					// We are not touching directories here that the user can specify explicitly.
					if (TypeInfo->SpecificAssets.Remove(AssetPath) > 0)
					{
						// Reset scan paths.
						RefreshAssetManagerTypeInfoList();
						TryUpdateDefaultConfigFile();
					}
				}

#endif // !UE_VERSION_OLDER_THAN(5, 4, 0)

				// Cleanup redirects.
				if (UCommonInventorySettings* const Settings = UCommonInventorySettings::GetMutable())
				{
					Settings->Modify(/* bAlwaysMarkDirty */ false);

					//We are not touching type redirects here.
					if (FCommonInventoryRedirects::CleanupRedirects(Settings->PrimaryAssetNameRedirects, PrimaryAssetId.PrimaryAssetName))
					{
						FCommonInventoryRedirects::Get().ForceRefresh();
						Settings->TryUpdateDefaultConfigFile();
					}
				}

				// Finally notify the registry.
				RegistryBridge->RemoveRecords({ PrimaryAssetId });
			}
		}
	}
}

void UAssetManagerDataSource::OnAssetRenamed(UCommonItemDefinition* InItemDefinition, const FSoftObjectPath& OldObjectPath, bool& bOutRedirectorFailed)
{
	check(RegistryBridge);

	if (InItemDefinition && CanRefreshRegistry())
	{
		const FPrimaryAssetId OldPrimaryAssetId = InItemDefinition->SharedData.PrimaryAssetId;

		if (const FCommonInventoryRegistryRecord* const Record = RegistryBridge->GetRegistryRecord(OldPrimaryAssetId); Record && Record->AssetPath == OldObjectPath)
		{
			const FPrimaryAssetId NewPrimaryAssetId = InItemDefinition->GetPrimaryAssetId();

#if !UE_VERSION_OLDER_THAN(5, 4, 0)

			// Fixup a path if it is from SpecificAssets.
			if (FCommonInventoryTypeInfo* const TypeInfo = Algo::FindBy(ScanTypeInfoList, NewPrimaryAssetId.PrimaryAssetType, &FCommonInventoryTypeInfo::PrimaryAssetType))
			{
				Modify(/* bAlwaysMarkDirty */ false);
				
				TypeInfo->SpecificAssets.Remove(OldObjectPath);
				AppendTypeInfoList(*TypeInfo, InItemDefinition);

				TryUpdateDefaultConfigFile();
				RefreshAssetManagerTypeInfoList();
			}

#endif // !UE_VERSION_OLDER_THAN(5, 4, 0)

			// Synchronize FPrimaryAssetId inside the definition.
			InItemDefinition->Modify(/* bAlwaysMarkDirty */ false);
			InItemDefinition->SharedData.PrimaryAssetId = InItemDefinition->GetPrimaryAssetId();

			// This will add a new record if it was actually a rename and not a move.
			if (RegistryBridge->AppendRecords({ FCommonInventoryRegistryRecord(*InItemDefinition) }) > 0)
			{
				// For now, we will always create a redirector, solving the following problems:
				// 1. How to avoid data loss in players' saves.
				// 2. How to avoid data loss in large teams with VCS. One developer renames items, another creates dependencies on old ones.
				// 3. How to avoid data loss for unsaved packages.
				if (UCommonInventorySettings* const Settings = UCommonInventorySettings::GetMutable())
				{
					Settings->Modify(/* bAlwaysMarkDirty */ false);

					if (FCommonInventoryRedirects::AppendRedirects(Settings->PrimaryAssetNameRedirects, OldPrimaryAssetId.PrimaryAssetName, NewPrimaryAssetId.PrimaryAssetName, true))
					{
						FCommonInventoryRedirects::Get().ForceRefresh();
						Settings->TryUpdateDefaultConfigFile();
					}
					else
					{
						bOutRedirectorFailed = true;
					}
				}

				// Finally, remove the old record and run the propagator.
				verify(RegistryBridge->RemoveRecords({ OldPrimaryAssetId }));
			}
		}
	}
}

void UAssetManagerDataSource::RefreshItemDefinition(UCommonItemDefinition* InItemDefinition)
{
	check(RegistryBridge);

	if (InItemDefinition && CanRefreshRegistry())
	{
		UAssetManager::Get().RefreshAssetData(InItemDefinition);

		// Check if registered.
		if (UAssetManager::Get().GetPrimaryAssetIdForObject(InItemDefinition).IsValid())
		{
			if (const FCommonInventoryRegistryRecord* const OriginalRecord = RegistryBridge->GetRegistryRecord(InItemDefinition->GetPrimaryAssetId()))
			{
				// Don't trigger the OnPostRefresh event unless necessary.
				if (const FCommonInventoryRegistryRecord NewRecord(*InItemDefinition); !NewRecord.HasIdenticalData(*OriginalRecord))
				{
					RegistryBridge->AppendRecords({ NewRecord });
				}
			}
		}
	}
}

void UAssetManagerDataSource::OnCookStarted()
{
	bIsPendingCook = true;
	CancelPendingRefresh();
	ForceRefresh(/* bSynchronous */ true);
}

void UAssetManagerDataSource::OnCookFinished()
{
	bIsPendingCook = false;

	if (!IsRunningCookCommandlet())
	{
		ForceRefresh(/* bSynchronous */ true);
	}
}

bool UAssetManagerDataSource::VerifyAssumptionsForCook(const ITargetPlatform* InTargetPlatform) const
{
	COMMON_INVENTORY_SCOPED_TRACE(UAssetManagerDataSource::VerifyAssumptionsForCook);
	
	bool bVerified = true; // Show all errors at once instead of iterating the cook process with early exits.

	if (ensure(RegistryBridge))
	{
		UAssetManager& AssetManager = UAssetManager::Get();

		// Per platform filtering might break network compatibility.
		for (const FCommonInventoryRegistryRecord& RegistryRecord : RegistryBridge->GetRegistryRecords())
		{
			if (const UObject* const PrimaryAsset = AssetManager.GetPrimaryAssetObject(RegistryRecord.GetPrimaryAssetId()))
			{
				const bool bNeedsLoad = PrimaryAsset->NeedsLoadForTargetPlatform(InTargetPlatform);

				if (!bNeedsLoad || !AssetManager.ShouldCookForPlatform(PrimaryAsset->GetPackage(), InTargetPlatform))
				{
					COMMON_INVENTORY_LOG(Error, "UAssetManagerDataSource: Per platform filtering for '%s' isn't supported as it can leave data artifacts.", *PrimaryAsset->GetFullName());
					bVerified = false;
				}
			}
			else
			{
				checkNoEntry();
			}
		}
	}

	return bVerified;
}

#endif // WITH_EDITOR
