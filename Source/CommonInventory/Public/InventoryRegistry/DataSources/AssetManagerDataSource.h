// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "InventoryRegistry/CommonInventoryRegistryDataSource.h"

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Set.h"
#include "Engine/AssetManagerTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPtr.h"

#include "AssetManagerDataSource.generated.h"

struct FAssetData;
struct FStreamableHandle;

class UCommonItemDefinition;

/**
 * Custom FPrimaryAssetTypeInfo adapted for UCommonItemDefinition.
 */
USTRUCT(meta = (Hidden))
struct FCommonInventoryTypeInfo
{
	GENERATED_BODY()

	/** The logical name for this type of Primary Asset. */
	UPROPERTY(EditDefaultsOnly, Category = "AssetType")
	FName PrimaryAssetType;

	/** Base Class of all assets of this type. */
	UPROPERTY(EditDefaultsOnly, Category = "AssetType", meta = (AllowAbstract))
	TSoftClassPtr<UCommonItemDefinition> AssetBaseClass;

	/**
	 * If true this type will not cause anything to be cooked; the AssetManager will use instances of this type to
	 * define chunk assignments and NeverCook rules, but will ignore AlwaysCook rules. Assets labeled by instances
	 * of this type will need to be reference by another PrimaryAsset, or by something outside the AssetManager,
	 * to be cooked.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "AssetType")
	bool bIsEditorOnly = false;

	/** Directories to search for this asset type. */
	UPROPERTY(EditDefaultsOnly, Category = "AssetType", meta = (RelativeToGameContentDir, LongPackageName))
	TArray<FDirectoryPath> Directories;

	/** Individual assets to scan. */
	UPROPERTY(EditDefaultsOnly, Category = "AssetType", meta = (Untracked, MetaClass = "/Script/CommonInventory.CommonItemDefinition", DisplayThumbnail = false))
	TArray<FSoftObjectPath> SpecificAssets;
};

/**
 * A persistent data source implementation for integrating UCommonItemDefinition with the registry through the asset manager.
 */
UCLASS(Config = Game, DefaultConfig)
class COMMONINVENTORY_API UAssetManagerDataSource : public UCommonInventoryRegistryDataSource
{
	GENERATED_BODY()

	UAssetManagerDataSource();

public: // Overrides

	virtual void Initialize(ICommonInventoryRegistryBridge* InRegistryBridge) override;
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;

	virtual void ForceRefresh(bool bSynchronous = false) override;
	virtual void FlushPendingRefresh() override;
	virtual void CancelPendingRefresh() override;
	virtual bool IsPendingRefresh() const override;
	virtual bool IsRefreshing() const override;

#if WITH_EDITOR
	virtual void OnCookStarted() override;
	virtual void OnCookFinished() override;
	virtual bool VerifyAssumptionsForCook(const ITargetPlatform* InTargetPlatform) const override;
#endif // WITH_EDITOR

public:

	void RefreshAssetManagerTypeInfoList();
	bool CanRefreshRegistry() const;

#if WITH_EDITOR
	
	// IAssetRegistry callbacks.
	virtual void OnAssetCreated(UCommonItemDefinition* InItemDefinition);
	virtual void OnAssetRemoved(const FAssetData& InRemovedAsset);
	virtual void OnAssetRenamed(UCommonItemDefinition* InItemDefinition, const FSoftObjectPath& OldObjectPath, bool& bOutRedirectorFailed);

	virtual void RefreshItemDefinition(UCommonItemDefinition* InItemDefinition);

#endif // WITH_EDITOR

protected:

	virtual void BulkRefresh();
	virtual TSharedPtr<FStreamableHandle> PreloadPrimaryAssets();
	virtual void ScanPrimaryAssetPaths(TArrayView<FPrimaryAssetTypeInfo> TypeInfoList);

#if WITH_EDITOR
	void AppendTypeInfoList(FCommonInventoryTypeInfo& InTypeInfo, const UCommonItemDefinition* InItemDefinition);
#endif // WITH_EDITOR

protected:

	template<typename T, int32 NumInlineElements>
	using TInlineSet = TSet<T, DefaultKeyFuncs<T>, TInlineSetAllocator<NumInlineElements>>;

	/** List of asset types to scan at startup. */
	UPROPERTY(Config)
	TArray<FCommonInventoryTypeInfo> ScanTypeInfoList;

	/** List of scanned asset types. */
	TInlineSet<FPrimaryAssetType, 16> RegisteredTypeInfoList;

	/** Assets loaded since the last refresh. */
	TSharedPtr<FStreamableHandle> PreloadHandle;
	
	/** Whether we should keep loaded assets. */
	UPROPERTY(Config)
	bool bKeepLoadedAssets;

	/** Mainly used because of GStreamableDelegateDelayFrames. */
	bool bIsPendingRefresh;

	/** Whether the registry is cooking. */
	bool bIsPendingCook;

	/** Whether refreshing is in progress. */
	bool bIsRefreshing;
};
