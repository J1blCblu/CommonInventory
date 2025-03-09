// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CommonInventoryTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UObject/PrimaryAssetId.h"

#include "CommonInventoryState.generated.h"

/**
 * Internal item implementation for UCommonInventoryComponent.
 *
 * @Note: The ReplicationID functionality is extended to ordering.
 */
USTRUCT(meta = (Hidden))
struct FCommonInventoryItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FCommonInventoryItem() = default;

	//bool operator==(const FCommonInventoryItem& Other) const;
	//bool operator!=(const FCommonInventoryItem& Other) const;

public: // Duplicating the hierarchy allows us to fit into 64 bytes in shipping builds.

	/**  */
	UPROPERTY()
	int32 StackSize = 0;

	/**  */
	UPROPERTY()
	FPrimaryAssetId PrimaryAssetId;

	/**  */
	UPROPERTY()
	FVariadicStruct ItemPayload;

public:

	/** Whether the item redirects onto another item. */
	bool IsRedirector() const { return PrimaryAssetId == FCommonInventoryRedirectorItem::GetPrimaryAssetId(); }

	/**  */
	int32 GetOffset() const { return ReplicationID; }

	/**  */
	void SetOffset(int32 InOffset) { ReplicationID = InOffset; }

public: // StructOpsTypeTraits

	COMMONINVENTORY_API bool Serialize(FArchive& Ar);
	COMMONINVENTORY_API bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FCommonInventoryItem> : public TStructOpsTypeTraitsBase2<FCommonInventoryItem>
{
	enum
	{
		WithCopy = true,
		//WithIdenticalViaEquality = true,
		//WithSerializer = true,
		//WithNetSerializer = true,
		WithNetSharedSerialization = true, // By definition, can't contain data per connection.
	};
};

/**
 * A set of chunk flags.
 * 
 * Some possible use cases of the 'Hidden' flag:
 * 1. Weapon has kits that buff it. We don't want to accidentally iterate over them during building the UI.
 * 2. Item with rarity buffs. Buffs are special sub-items which we don't want to be iterated implicitly. We can read them explicitly while hovering a mouse over the item.
 * 3. Exclude notes during the total weight calculation.
 */
UENUM()
enum class ECommonInventoryStateFlags : uint16
{
	NoFlags = 0 << 0,

	Automatic		= 1 << 0, // The Chunk will implicitly reserve capacity when it's exhausted.
	StackUnlimited	= 1 << 1, // Ignore MaxStackSize restrictions.
	NoDuplicates	= 1 << 2, // Disallow duplicates.

	All			= (1 << 8) - 1,
};

ENUM_CLASS_FLAGS(ECommonInventoryStateFlags);

/**
 * 
 */
USTRUCT(meta = (Hidden))
struct COMMONINVENTORY_API FCommonInventoryState : public FFastArraySerializer
{
	GENERATED_BODY()

	FCommonInventoryState() = default;

public:

	/**  */
	void Initialize(uint32 InInitialCapacity);

	/**  */
	void Deinitialize();

public: // StructOpsTypeTraits

	bool Serialize(FArchive& Ar);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FCommonInventoryItem, FCommonInventoryState>(Items, DeltaParms, *this);
	}

	// FFastArraySerializer.
	void PostReplicatedReceive(FFastArraySerializer::FPostReplicatedReceiveParameters PostReceivedParameters) {}
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize) {}
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize) {}
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize) {}

private:

	/**  */
	UPROPERTY()
	TArray<FCommonInventoryItem> Items;

	/**  */
	//TBitArray NetTransactionalFlags;

	/**  */
	UPROPERTY()
	uint32 Size = 0;

	/**  */
	UPROPERTY()
	uint32 Capacity = 0;

	/**  */
	UPROPERTY()
	ECommonInventoryStateFlags InternalFlags = ECommonInventoryStateFlags::NoFlags;
};

template<>
struct TStructOpsTypeTraits<FCommonInventoryState> : public TStructOpsTypeTraitsBase2<FCommonInventoryState>
{
	enum
	{
		//WithSerializer = true,
		WithNetDeltaSerializer = true,
		WithNetSharedSerialization = true,
	};
};

/**  */
enum class ECommonInventoryStateEventType
{
	ItemAdded,
	ItemRemoved,
	ItemUpdated,
};

/**
 * 
 */
struct FCommonInventoryStateChangeTrackerContext
{

};

/**
 * 
 */
struct FCommonInventoryStateChangeTracker
{

};
