// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "InventoryRegistry/CommonInventoryRegistryTypes.h"

#include "CoreTypes.h"
#include "Containers/ArrayView.h"
#include "UObject/PrimaryAssetId.h"

/**
 * A simple interface that allows a data source to act as a single source of authority for interacting with the registry.
 */
class ICommonInventoryRegistryBridge
{
public:

	/** Returns a pointer to the registry record from FPrimaryAssetId. */
	virtual const FCommonInventoryRegistryRecord* GetRegistryRecord(FPrimaryAssetId InPrimaryAssetId) const = 0;

	/** Returns all records, or records of the specified type if Archetype is provided. */
	virtual TConstArrayView<FCommonInventoryRegistryRecord> GetRegistryRecords(FPrimaryAssetType InArchetype = FPrimaryAssetType()) const = 0;

	/** Appends new records or updates the existing ones. */
	virtual int32 AppendRecords(TConstArrayView<FCommonInventoryRegistryRecord> InRecords) = 0;

	/** Removes records from the registry. */
	virtual int32 RemoveRecords(TConstArrayView<FPrimaryAssetId> InRecordIds) = 0;

	/** Full resets the registry state. */
	virtual void ResetRecords(TConstArrayView<FCommonInventoryRegistryRecord> InRecords) = 0;

	/** Whether the registry was successfully loaded from disk. */
	virtual bool WasLoaded() const = 0;

#if WITH_EDITOR

	/** Whether the registry is in the cooking mode. */
	virtual bool IsCooking() const = 0;

#endif // WITH_EDITOR

protected:

	/** Intentionally not virtual. Only subclasses are allowed to expose this interface to authorized clients. */
	~ICommonInventoryRegistryBridge() = default;
};
