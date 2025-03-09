// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CommonInventoryTypes.h"
#include "CommonItemDefinition.h"

#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "InstancedStructContainer.h"
#include "StructView.h"
#include "Templates/Function.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/PrimaryAssetId.h"
#include "CommonInventoryRegistryTypes.generated.h"

class FArchive;
struct FCommonInventoryRedirector;

namespace CommonInventory
{
	// Reserved index for invalid items.
	inline constexpr uint32 INVALID_REPLICATION_INDEX = 0;
}

/**
 * A struct wrapper over shared data with registry metadata.
 */
USTRUCT(meta = (Hidden))
struct FCommonInventoryRegistryRecord
{
	GENERATED_BODY()

	FCommonInventoryRegistryRecord() = default;
	
	FCommonInventoryRegistryRecord(const FCommonItemSharedData& InSharedData, FConstStructView InDefaultPayload = FConstStructView(), FConstStructView InCustomData = FConstStructView())
		: DefaultPayload(InDefaultPayload)
		, CustomData(InCustomData)
		, SharedData(InSharedData)
	{
		// Do something.
	}

	FCommonInventoryRegistryRecord(const UCommonItemDefinition& InDefinition)
		: DefaultPayload(InDefinition.DefaultItemPayload)
		, CustomData(InDefinition.RegistryCustomData)
		, SharedData(InDefinition.SharedData)
		, AssetPath(&InDefinition)
	{
		SharedData.PrimaryAssetId = InDefinition.GetPrimaryAssetId();
	}

	COMMONINVENTORY_API friend bool operator<(const FCommonInventoryRegistryRecord& Lhs, const FCommonInventoryRegistryRecord& Rhs);

public:

	/** Cached view of default payload. */
	FConstStructView DefaultPayload;

	/** Cached view of custom data. */
	FConstStructView CustomData;

	/** The actual cached shared data. */
	UPROPERTY()
	FCommonItemSharedData SharedData;

	/** Original asset path the data copied from. */
	UPROPERTY()
	FSoftObjectPath AssetPath;

	/** Optional index to dedicated default payload. */
	UPROPERTY()
	int32 DefaultPayloadIndex = INDEX_NONE;

	/** Optional index to dedicated user data. */
	UPROPERTY()
	int32 CustomDataIndex = INDEX_NONE;

	/** Replication index. */
	uint32 RepIndex = CommonInventory::INVALID_REPLICATION_INDEX;

private:

	/** Cached Crc32 hash. */
	mutable uint32 Checksum = 0;

public:

	bool IsValid() const
	{
		return SharedData.PrimaryAssetId.IsValid();
	}

	FPrimaryAssetId GetPrimaryAssetId() const
	{
		return SharedData.PrimaryAssetId;
	}

	FPrimaryAssetType GetPrimaryAssetType() const
	{
		return GetPrimaryAssetId().PrimaryAssetType;
	}

	FName GetPrimaryAssetName() const
	{
		return GetPrimaryAssetId().PrimaryAssetName;
	}

	/** Returns a lazily calculated checksum. */
	COMMONINVENTORY_API uint32 GetChecksum() const;

	/** Invalidates the cached checksum. */
	void InvalidateChecksum()
	{
		Checksum = 0;
	}

	/** Whether the records are identical excluding metadata. */
	COMMONINVENTORY_API bool HasIdenticalData(const FCommonInventoryRegistryRecord& Other) const;

	/**	A faster version of the above, but requires a valid checksum and is therefore less accurate. */
	bool HasIdenticalDataFast(const FCommonInventoryRegistryRecord& Other) const
	{
		return Checksum == Other.Checksum;
	}

	/** Dumps the internal record state into the log. */
	COMMONINVENTORY_API void Dump() const;
};

/**
 * Internal representation of the registry state.
 */
USTRUCT(meta = (Hidden))
struct FCommonInventoryRegistryState
{
	GENERATED_BODY()

	//@TODO: Maybe just use FName in TMap without FPrimaryAssetType since the name should be unique anyway.

	FCommonInventoryRegistryState() = default;
	FCommonInventoryRegistryState(FCommonInventoryRegistryState&& Other) = default;
	FCommonInventoryRegistryState(const FCommonInventoryRegistryState& Other) = default;
	FCommonInventoryRegistryState& operator=(const FCommonInventoryRegistryState& Other) = default;

#if UE_VERSION_OLDER_THAN(5, 5, 0)
	// Prior to 5.5.0, FInstancedStructContainer had a broken move-assignment operator.
	COMMONINVENTORY_API FCommonInventoryRegistryState& operator=(FCommonInventoryRegistryState&& Other);
#else
	FCommonInventoryRegistryState& operator=(FCommonInventoryRegistryState&& Other) = default;
#endif // UE_VERSION_OLDER_THAN(5, 5, 0)

public: // Management

	/** Resets the registry state. */
	COMMONINVENTORY_API void Reset(TConstArrayView<FCommonInventoryRegistryRecord> InRegistryData = TConstArrayView<FCommonInventoryRegistryRecord>());

	/** Updates existing data or creates a new one. */
	COMMONINVENTORY_API bool AppendData(const FCommonInventoryRegistryRecord& InRegistryData);

	/** Removes data from the state. */
	COMMONINVENTORY_API bool RemoveData(FPrimaryAssetId PrimaryAssetId);

public: // Access

	/** Whether the registry contains any data. */
	bool HasRecords() const { return !DataContainer.IsEmpty(); }

	/** Whether the registry contains a record for FPrimaryAssetId. */
	bool ContainsRecord(FPrimaryAssetId PrimaryAssetId) const { return DataMap.Contains(PrimaryAssetId); }

	/** Whether the registry contains an archetype. */
	bool ContainsArchetype(FPrimaryAssetType PrimaryAssetType) const { return FindArchetypeGroup(PrimaryAssetType) != nullptr; }

	/** Returns the registry record for FPrimaryAssetId. */
	const FCommonInventoryRegistryRecord& GetRecord(FPrimaryAssetId PrimaryAssetId) const { return DataContainer[DataMap.FindChecked(PrimaryAssetId)]; }

	/** Returns the registry record pointer for FPrimaryAssetId. */
	const FCommonInventoryRegistryRecord* GetRecordPtr(FPrimaryAssetId PrimaryAssetId) const
	{
		if (const int32* const Idx = DataMap.Find(PrimaryAssetId))
		{
			return &DataContainer[*Idx];
		}

		return nullptr;
	}

	/** Returns the registry record pointer from RepIndex. */
	const FCommonInventoryRegistryRecord* GetRecordFromReplication(uint32 RepIndex) const
	{
		if (const int32* const Idx = ReplicationMap.Find(RepIndex))
		{
			return &DataContainer[*Idx];
		}

		return nullptr;
	}

	/** Returns the total number of records, or the number of records of the specified type if Archetype is provided. */
	int32 GetRecordsNum(FPrimaryAssetType InArchetype = FPrimaryAssetType()) const
	{
		if (InArchetype.IsValid())
		{
			if (const FArchetypeGroup* const TypeGroup = FindArchetypeGroup(InArchetype))
			{
				return TypeGroup->Offset;
			}

			return 0;
		}

		return DataContainer.Num();
	}

	/** Returns all records, or records of the specified type if Archetype is provided.  */
	TArrayView<const FCommonInventoryRegistryRecord> GetRecords(FPrimaryAssetType InArchetype = FPrimaryAssetType()) const
	{
		if (InArchetype.IsValid())
		{
			if (const FArchetypeGroup* const TypeGroup = FindArchetypeGroup(InArchetype))
			{
				return MakeArrayView(DataContainer.GetData() + TypeGroup->Begin, TypeGroup->Offset);
			}

			return TArrayView<const FCommonInventoryRegistryRecord>();
		}

		return MakeArrayView(DataContainer);
	}

	/** Returns all record ids, or record ids of the specified type if Archetype is provided. */
	template<typename Allocator>
	void GetRecordIds(TArray<FPrimaryAssetId, Allocator>& OutRecordIds, FPrimaryAssetType InArchetype = FPrimaryAssetType()) const
	{
		TArrayView RecordsView = GetRecords(InArchetype);
		OutRecordIds.Reserve(OutRecordIds.Num() + RecordsView.Num());
		Algo::Transform(RecordsView, OutRecordIds, &FCommonInventoryRegistryRecord::GetPrimaryAssetId);
	}

	/** Returns all registered archetypes. */
	template<typename Allocator>
	void GetArchetypes(TArray<FPrimaryAssetType, Allocator>& OutArchetypes) const
	{
		OutArchetypes.Reserve(OutArchetypes.Num() + Archetypes.Num());
		Algo::Transform(Archetypes, OutArchetypes, &FArchetypeGroup::PrimaryAssetType);
	}

public: // Utils

	/** Returns number of bits to encode RepIndex. */
	int64 GetRepIndexEncodingBitsNum() const { return RepIndexEncodingBitsNum; }

	/** Returns a lazily calculated Crc32 checksum excluding metadata. Can be used to validate network compatibility, etc. */
	COMMONINVENTORY_API uint32 GetChecksum() const;

	/** Writes the state directly into a file. */
	COMMONINVENTORY_API bool SaveToFile(const FString& Filename, bool bIsCooking = false);

	/** Writes the state into FArchive. */
	COMMONINVENTORY_API bool SaveState(FArchive& Ar, bool bIsCooking = false);

	/** Reads the state directly from a file. */
	COMMONINVENTORY_API bool LoadFromFile(const FString& Filename, bool bIsCooked = false);

	/** Reads the state from FArchive. */
	COMMONINVENTORY_API bool LoadState(FArchive& Ar, bool bIsCooked = false);

	/** Removes all the records identical to the records from the base state. */
	COMMONINVENTORY_API void DiffRecords(const FCommonInventoryRegistryState& InBaseState);

	/** Dumps the internal registry state into the log. */
	COMMONINVENTORY_API void Dump() const;

protected:

	/** A struct wrapper over a group of records of the same type. */
	struct FArchetypeGroup
	{
		FPrimaryAssetType PrimaryAssetType;
		int32 Begin = INDEX_NONE;
		int32 Offset = 0;
		mutable uint32 Checksum = 0;
	};

	const FArchetypeGroup* FindArchetypeGroup(FPrimaryAssetType PrimaryAssetType) const
	{
		return Algo::FindBy(Archetypes, PrimaryAssetType, &FArchetypeGroup::PrimaryAssetType);
	}

	FArchetypeGroup* FindArchetypeGroup(FPrimaryAssetType PrimaryAssetType)
	{
		return Algo::FindBy(Archetypes, PrimaryAssetType, &FArchetypeGroup::PrimaryAssetType);
	}

	void FixupDependencies(bool bMigrateArchetypeChecksum = false);
	void RemoveCustomData(int32 InCustomDataIndex);

private:

	/** Storage for registry records with metadata. */
	UPROPERTY()
	TArray<FCommonInventoryRegistryRecord> DataContainer;

	/** Shared storage for default payload and custom data. */
	UPROPERTY()
	FInstancedStructContainer CustomDataContainer;

	/** FPrimaryAssetType related data. */
	TArray<FArchetypeGroup, TInlineAllocator<12>> Archetypes;

	/** Maps FPrimaryAssetIds to DataContainer indices. */
	TMap<FPrimaryAssetId, int32> DataMap;

	/** Maps RepIndex to DataContainer indices. */
	TMap<uint32, int32> ReplicationMap;

	/** Number of bits to encode RepIndex. */
	int64 RepIndexEncodingBitsNum = 0;

	/** Crc32 checksum excluding metadata. */
	mutable uint32 Checksum = 0;
};

/**
 * Encapsulates FPrimaryAssetType and FPrimaryAssetName redirects.
 * Supports chain collapsing, eliminates cyclic and ambiguous dependencies.
 */
struct FCommonInventoryRedirects
{
	COMMONINVENTORY_API static FCommonInventoryRedirects& Get();

	/** Returns the resolved redirection map. */
	const TMap<FName, FName>& GetTypeRedirects() const { return TypeRedirectionMap; }
	const TMap<FName, FName>& GetNameRedirects() const { return NameRedirectionMap; }

#if WITH_EDITOR

	/** Force reload redirects from the config. */
	COMMONINVENTORY_API void ForceRefresh();

#endif // WITH_EDITOR

public:
	
	/** Whether FPrimaryAssetType is stale and has an active redirector. */
	COMMONINVENTORY_API bool IsStale(FPrimaryAssetType InPrimaryAssetType) const;

	/** Whether FPrimaryAssetId is stale and has an active redirector. */
	COMMONINVENTORY_API bool IsStale(FPrimaryAssetId InPrimaryAssetId) const;

	/** Tries to redirect FPrimaryAssetType. */
	COMMONINVENTORY_API bool TryRedirect(FPrimaryAssetType& InPrimaryAssetType) const;

	/** Tries to redirect FPrimaryAssetId. */
	COMMONINVENTORY_API bool TryRedirect(FPrimaryAssetId& InPrimaryAssetId) const;

	/** Whether FPrimaryAssetType has any redirects. */
	COMMONINVENTORY_API bool HasTypeRedirects(FPrimaryAssetType InPrimaryAssetType) const;

	/** Whether FPrimaryAssetName has any redirects. */
	COMMONINVENTORY_API bool HasNameRedirects(FName InPrimaryAssetName) const;

	/**
	 * Traverses over permutations of type and name redirects.
	 * Without timestamps, it is impossible to reconstruct the original redirection chain.
	 * But it's guaranteed that the resulting values don't overlap with unrelated values.
	 */
	COMMONINVENTORY_API void TraversePermutations(FPrimaryAssetId InPrimaryAssetId, TFunctionRef<bool(FPrimaryAssetId)> InPredicate) const;

public: // Static Helpers

	/** Whether OriginalValue can be resolved into TargetValue. */
	COMMONINVENTORY_API static bool CanResolveInto(TArrayView<const FCommonInventoryRedirector> InRedirects, FName OriginalValue, FName TargetValue);

	/** Whether the values can be resolved into the same value. */
	COMMONINVENTORY_API static bool HasCommonBase(TArrayView<const FCommonInventoryRedirector> InRedirects, FName FirstValue, FName SecondValue);

	/** Tries to insert a new redirector. If the inserted redirector creates a circular dependency, bInvertCircularDependency inverts the intermediate redirects instead. */
	COMMONINVENTORY_API static bool AppendRedirects(TArray<FCommonInventoryRedirector>& InRedirects, FName OldValue, FName NewValue, bool bInvertCircularDependency = false);

	/** Clears all redirects that resolve into TargetValue. Returns true if there were any removals. */
	COMMONINVENTORY_API static bool CleanupRedirects(TArray<FCommonInventoryRedirector>& InRedirects, FName TargetValue, bool bRecursively = true);

private:

	FCommonInventoryRedirects();

	TMap<FName, FName> TypeRedirectionMap;
	TMap<FName, FName> NameRedirectionMap;
};

/**
 * A simple helper for propagating defaults from the registry across reflected types.
 */
struct FCommonInventoryDefaultsPropagator
{
	COMMONINVENTORY_API static FCommonInventoryDefaultsPropagator& Get();

	/** Some context data to facilitate data migration. */
	struct FContext
	{
		/** The original registry state slice. */
		FCommonInventoryRegistryState OriginalRegistryState;

		/** Objects visited during the propagation. Mostly used to avoid recurring. */
		TSet<UObject*> VisitedObjects;

		/** The current object the propagation is performed on. */
		UObject* CurrentObject = nullptr;

		/** Whether the initial fixup is running. */
		bool bIsInitialFixup = false;

		/** Whether the registry was fully reset. */
		bool bWasReset = false;
	};

	DECLARE_DELEGATE_ThreeParams(FGatherObjectsOverrideDelegate, const FContext& /* InContext */, TArray<UObject*>& /* OutObjects */, bool& /* bOutSkipGathering */);
	DECLARE_MULTICAST_DELEGATE_OneParam(FDefaultsPropagationDelegate, const FContext& /* InContext */);

public: // Events

	/** Register a delegate to override the default objects gathering behavior. */
	void Bind_OnGatherObjectsOverride(FGatherObjectsOverrideDelegate&& Delegate)
	{
		GatherObjectsOverrideDelegate = MoveTemp(Delegate);
	}

	void Unbind_OnGatherObjectsOverride()
	{
		GatherObjectsOverrideDelegate.Unbind();
	}

	/** Register a delegate to be called right before propagating defaults. */
	FDelegateHandle Register_OnPrePropagateDefaults(FDefaultsPropagationDelegate::FDelegate&& Delegate)
	{
		return PreDefaultsPropagationDelegate.Add(MoveTemp(Delegate));
	}

	void Unregister_OnPrePropagateDefaults(FDelegateHandle Handle)
	{
		PreDefaultsPropagationDelegate.Remove(Handle);
	}

	/** Register a delegate to be called right after propagating defaults. */
	FDelegateHandle Register_OnPostPropagateDefaults(FDefaultsPropagationDelegate::FDelegate&& Delegate)
	{
		return PostDefaultsPropagationDelegate.Add(MoveTemp(Delegate));
	}

	void Unregister_OnPostPropagateDefaults(FDelegateHandle Handle)
	{
		PostDefaultsPropagationDelegate.Remove(Handle);
	}

public: // Core

	/** Whether the defaults propagation is in progress. */
	bool IsPropagatingDefaults() const { return bIsPropagatingDefaults.load(std::memory_order::relaxed); }

	/** Can be used to extract propagation context from the archive. */
	COMMONINVENTORY_API const FContext* GetContextFromArchive(FArchive& Ar) const;

	/** Propagates new defaults across reflected types. Any pending async loads will be flushed. */
	COMMONINVENTORY_API void PropagateRegistryDefaults(FContext& InContext);

	/** Gathers objects for defaults propagation. */
	COMMONINVENTORY_API bool GatherObjectsForPropagation(const FContext& InContext, TArray<UObject*>& OutObjects);

private:

	FCommonInventoryDefaultsPropagator() = default;

	FGatherObjectsOverrideDelegate GatherObjectsOverrideDelegate;
	FDefaultsPropagationDelegate PreDefaultsPropagationDelegate;
	FDefaultsPropagationDelegate PostDefaultsPropagationDelegate;
	std::atomic<bool> bIsPropagatingDefaults = false;
};

// Defaults propagation context alias.
using FCommonInventoryDefaultsPropagationContext = FCommonInventoryDefaultsPropagator::FContext;

/**
 * A struct wrapper to make NetSerialization interface safer.
 */
struct FCommonInventoryRegistryNetSerializationContext
{
private:
	friend class UCommonInventoryRegistry;
	FCommonInventoryRegistryNetSerializationContext(FPrimaryAssetId InPrimaryAssetId, bool& bOutSuccess)
		: PrimaryAssetId(InPrimaryAssetId), bOutSuccess(bOutSuccess)
	{
	}

	const FCommonInventoryRegistryRecord* RegistryRecord = nullptr;
	const FPrimaryAssetId PrimaryAssetId;
	bool& bOutSuccess;
};
