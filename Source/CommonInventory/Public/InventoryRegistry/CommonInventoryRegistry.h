// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "InventoryRegistry/CommonInventoryRegistryBridge.h"
#include "InventoryRegistry/CommonInventoryRegistryDataSource.h"
#include "InventoryRegistry/CommonInventoryRegistryTypes.h"

#include "Engine/AssetManagerTypes.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/EngineSubsystem.h"
#include "UObject/PrimaryAssetId.h"
#include "CommonInventoryRegistry.generated.h"

#ifndef COMMON_INVENTORY_WITH_NETWORK_CHECKSUM
#define COMMON_INVENTORY_WITH_NETWORK_CHECKSUM (!(UE_BUILD_SHIPPING || UE_BUILD_TEST))
#endif

class UCommonInventoryRegistryDataSource;

/**
 * A simple registry for storing shared data in a single place and memory block.
 * 
 * Provides various useful utilities including:
 * - Serialization including FPrimaryAssetType/FPrimaryAssetName redirections. Supports the async loading thread.
 * - Optimized network serialization with desync detection.
 * - Defaults propagation across reflected types after refreshes.
 * - Tracking dependencies in packages via searchable names.
 * - Optional registry cooking to reduce startup overhead.
 * 
 * List of console commands:
 * 1. CommonInventory.DumpRegistry - Shows a summary of all registered items or specific items parsed from arguments.
 * 2. CommonInventory.ForceRefreshRegistry - Force refresh the registry state.
 */
UCLASS(Transient, Hidden)
class COMMONINVENTORY_API UCommonInventoryRegistry final : public UEngineSubsystem
#if CPP
	, private ICommonInventoryRegistryBridge // Only exposed to UCommonInventoryRegistryDataSource.
#endif
{
	GENERATED_BODY()

public:

	UCommonInventoryRegistry() = default;

	/** Accessors. */
	static bool IsInitialized();
	static UCommonInventoryRegistry& Get();
	static UCommonInventoryRegistry* GetPtr();

	/** Returns the filename for InventoryRegistry.bin. */
	static FString GetRegistryFilename();
	static FString GetDevelopmentRegistryFilename();

public: // Events

	/** Register a delegate to be called once the registry has initialized. */
	static void CallOrRegister_OnInventoryRegistryInitialized(FSimpleMulticastDelegate::FDelegate&& Delegate);

	/** Register a delegate to be called each time the registry is refreshed. */
	FDelegateHandle Register_OnPostRefresh(FSimpleMulticastDelegate::FDelegate&& Delegate)
	{
		return PostRefreshDelegate.Add(MoveTemp(Delegate));
	}

	/** Remove a previously registered delegate. */
	void Unregister_OnPostRefresh(FDelegateHandle Handle)
	{
		PostRefreshDelegate.Remove(Handle);
	}

public: // Data Access

	/** Whether the registry contains a record for FPrimaryAssetId. */
	bool ContainsRecord(FPrimaryAssetId InPrimaryAssetId) const
	{
		return RegistryState.ContainsRecord(InPrimaryAssetId);
	}

	/** Whether the registry contains an archetype. */
	bool ContainsArchetype(FPrimaryAssetType InPrimaryAssetType) const
	{
		return RegistryState.ContainsArchetype(InPrimaryAssetType);
	}

	/** Returns the registry record for FPrimaryAssetId. */
	const FCommonInventoryRegistryRecord& GetRegistryRecordChecked(FPrimaryAssetId InPrimaryAssetId) const
	{
		return RegistryState.GetRecord(InPrimaryAssetId);
	}

	/** Returns the registry record pointer for FPrimaryAssetId. */
	virtual const FCommonInventoryRegistryRecord* GetRegistryRecord(FPrimaryAssetId InPrimaryAssetId) const override final
	{
		return RegistryState.GetRecordPtr(InPrimaryAssetId);
	}

	/** Returns the registry record pointer from FPrimaryAssetName. */
	const FCommonInventoryRegistryRecord* FindRegistryRecordFromName(FName InItemName) const
	{
		return Algo::FindBy(GetRegistryRecords(), InItemName, &FCommonInventoryRegistryRecord::GetPrimaryAssetName);
	}

	/** Returns all records, or records of the specified type if Archetype is provided. */
	virtual TConstArrayView<FCommonInventoryRegistryRecord> GetRegistryRecords(FPrimaryAssetType InArchetype = FPrimaryAssetType()) const override final
	{
		return RegistryState.GetRecords(InArchetype);
	}

	/** Returns all record ids, or record ids of the specified type if Archetype is provided. */
	template<typename Allocator>
	void GetRecordIds(TArray<FPrimaryAssetId, Allocator>& OutRecordIds, FPrimaryAssetType InArchetype = FPrimaryAssetType()) const
	{
		RegistryState.GetRecordIds(OutRecordIds, InArchetype);
	}

	/** Returns all registered archetypes. */
	template<typename Allocator>
	void GetArchetypes(TArray<FPrimaryAssetType, Allocator>& OutArchetypes) const
	{
		RegistryState.GetArchetypes(OutArchetypes);
	}

public: // Item Utils

	/** Resets the item to its default state. */
	bool ResetItem(FPrimaryAssetId InPrimaryAssetId, FVariadicStruct& InPayload) const;

	/** Whether FPrimaryAssetId is synchronized with the payload. */
	bool ValidateItem(FPrimaryAssetId InPrimaryAssetId, const FVariadicStruct& InPayload) const;

	/** Redirects FPrimaryAssetId and synchronizes with the default payload. */
	bool SynchronizeItem(FPrimaryAssetId& InPrimaryAssetId, FVariadicStruct& InPayload) const;

	/** Serializes FPrimaryAssetId with the payload. */
	bool SerializeItem(FArchive& Ar, FPrimaryAssetId& InPrimaryAssetId, FVariadicStruct& InPayload) const;

	/** Propagates new defaults from the registry. */
	void PropagateItemDefaults(const FCommonInventoryDefaultsPropagator::FContext& InContext, FPrimaryAssetId& InPrimaryAssetId, FVariadicStruct& InPayload) const;

	/** Efficiently serializes FPrimaryAssetId. For a payload serialization use the returned context. */
	FCommonInventoryRegistryNetSerializationContext NetSerializeItem(FArchive& Ar, FPrimaryAssetId& InPrimaryAssetId, bool& bOutSuccess) const;

	/** Efficiently serializes the item payload with the context returned from NetSerializeItem(). */
	void NetSerializeItemPayload(FArchive& Ar, FVariadicStruct& InPayload, FCommonInventoryRegistryNetSerializationContext& InContext) const;

public: // Registry Utils

	/** Returns the registry data source. */
	UCommonInventoryRegistryDataSource* GetDataSource() const { return DataSource; }

	/** Returns the internal registry state. */
	const FCommonInventoryRegistryState& GetRegistryState() const { return RegistryState; }

	/** Force refresh the registry state. */
	void ForceRefresh(bool bSynchronous = false);

	/** Flushes any pending refreshes. */
	void FlushPendingRefresh();

	/** Cancels any pending refreshes. */
	void CancelPendingRefresh();

	/** Whether the registry is pending refresh. */
	bool IsPendingRefresh() const;

	/** Whether refreshing is in progress. */
	bool IsRefreshing() const;

#if WITH_EDITOR

	/** Whether the registry is in the cooking mode. */
	virtual bool IsCooking() const override { return bIsCooking; }

	/** Put the registry into the cooking mode. */
	void OnCookStarted();

	/** Put the registry back into the base mode. */
	void OnCookFinished();

	/** Write the cooked registry state into the file. */
	void WriteForCook(const ITargetPlatform* InTargetPlatform, const FString& InFilename);

	/** Reports any issues into the log. */
	void ReportInvariantViolation() const;

#endif // WITH_EDITOR

private:

	//~ Begin UEngineSubsystem Interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEngineSubsystem Interface

	void PostInitialize();
	bool TryLoadFromFile();

	void CheckDataSourceContractViolation() const;

	//~ Begin ICommonInventoryRegistryBridge Interface
	virtual int32 AppendRecords(TConstArrayView<FCommonInventoryRegistryRecord> InRecords) override;
	virtual int32 RemoveRecords(TConstArrayView<FPrimaryAssetId> InRecordIds) override;
	virtual void ResetRecords(TConstArrayView<FCommonInventoryRegistryRecord> InRecords) override;
	virtual bool WasLoaded() const override { return bWasLoaded; }
	//~ End ICommonInventoryRegistryBridge Interface

	void OnPostRefresh(FCommonInventoryDefaultsPropagationContext& InPropagationContext);
	void ConditionallyUpdateNetworkChecksum();

private:

	/** The data source responsible for gathering and managing data. */
	UPROPERTY()
	TObjectPtr<UCommonInventoryRegistryDataSource> DataSource;

	/** The internal registry state. */
	UPROPERTY()
	FCommonInventoryRegistryState RegistryState;

	/** The RegistryState guard for supporting the async loading thread. */
	mutable FCriticalSection CriticalSection;

	/** Delegate for broadcasting registry updates. */
	FSimpleMulticastDelegate PostRefreshDelegate;

	/** Cached data source traits from CDO. */
	FCommonInventoryRegistryDataSourceTraits DataSourceTraits;

	/** Whether the registry was loaded from disk. */
	bool bWasLoaded = false;

#if WITH_EDITOR

	/** Whether the registry is in the cooking mode. */
	bool bIsCooking = false;

#endif
};
