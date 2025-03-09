// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CommonInventoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CommonInventoryFunctionLibrary.generated.h"

UENUM()
enum class ECommonInventoryResult : uint8
{
	Valid,
	NotValid,
};

// Delegates for implementing algorithms in future.
//DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(bool, FCommonInventoryPredicateDelegate, const FCommonItem&, InCommonItem);
//DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(bool, FCommonInventoryComparatorDelegate, const FCommonItem&, Lhs, const FCommonItem&, Rhs);

/**
 * Common Inventory Function Library.
 */
UCLASS(MinimalAPI)
class UCommonInventoryFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	// To suppress C4263 and C4264
	using Super::GetPrimaryAssetId;

public: // UInventoryRegistry

	/** Whether the registry contains a record for FPrimaryAssetId. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|InventoryRegistry")
	static bool ContainsRecord(FPrimaryAssetId InPrimaryAssetId);

	/** Whether the registry contains an archetype. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|InventoryRegistry")
	static bool ContainsArchetype(FPrimaryAssetType InArchetype);

	/** Returns all record ids, or record ids of the specified type if Archetype is provided. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|InventoryRegistry")
	static void GetRecordIds(TArray<FPrimaryAssetId>& OutPrimaryAssetIds, FPrimaryAssetType InArchetype);

	/** Returns all registered archetypes. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|InventoryRegistry")
	static void GetArchetypes(TArray<FPrimaryAssetType>& OutArchetypes);

	/** Returns cached SharedData from the registry. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|InventoryRegistry", meta = (ExpandEnumAsExecs = "ReturnValue"))
	static ECommonInventoryResult GetRegistrySharedData(FPrimaryAssetId InPrimaryAssetId, FCommonItemSharedData& OutSharedData);

	/** Returns cached AssetPath from the registry. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|InventoryRegistry", meta = (ExpandEnumAsExecs = "ReturnValue"))
	static ECommonInventoryResult GetRegistryAssetPath(FPrimaryAssetId InPrimaryAssetId, FSoftObjectPath& OutAssetPath);

	/** Returns cached DefaultPayload from the registry. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "CommonInventory|InventoryRegistry", meta = (CustomStructureParam = "Value", ExpandEnumAsExecs = "ReturnValue", BlueprintInternalUseOnly = "true"))
	static ECommonInventoryResult GetRegistryDefaultPayload(FPrimaryAssetId InPrimaryAssetId, int32& Value);

	/** Returns cached CustomData from the registry. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "CommonInventory|InventoryRegistry", meta = (CustomStructureParam = "Value", ExpandEnumAsExecs = "ReturnValue", BlueprintInternalUseOnly = "true"))
	static ECommonInventoryResult GetRegistryCustomData(FPrimaryAssetId InPrimaryAssetId, int32& Value);

private:

	DECLARE_FUNCTION(execGetRegistryDefaultPayload);
	DECLARE_FUNCTION(execGetRegistryCustomData);

public: // FCommonItem

	/** Constructs FCommonItem from FPrimaryAssetId. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|CommonItem")
	static FCommonItem MakeCommonItem(FPrimaryAssetId InPrimaryAssetId = FPrimaryAssetId())
	{
		return FCommonItem(InPrimaryAssetId);
	}

	/** Whether the item is valid. */
	UFUNCTION(BlueprintPure, Category = "CommonInventory|CommonItem", meta = (BlueprintAutocast))
	static bool IsValid(const FCommonItem& InCommonItem)
	{
		return InCommonItem.IsValid();
	}

	/** Returns FPrimaryAssetId of the item. */
	UFUNCTION(BlueprintPure, Category = "CommonInventory|CommonItem", meta = (BlueprintAutocast))
	static FPrimaryAssetId GetPrimaryAssetId(const FCommonItem& InCommonItem)
	{
		return InCommonItem.GetPrimaryAssetId();
	}

	/** Updates FPrimaryAssetId of the item and resets payload. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|CommonItem")
	static void SetPrimaryAssetId(UPARAM(Ref) FCommonItem& InCommonItem, FPrimaryAssetId InPrimaryAssetId)
	{
		InCommonItem.SetPrimaryAssetId(InPrimaryAssetId);
	}

	/** Whether the item contains a valid payload object. */
	UFUNCTION(BlueprintPure, Category = "CommonInventory|CommonItem")
	static bool HasPayload(const FCommonItem& InCommonItem)
	{
		return InCommonItem.HasPayload();
	}

	/** Returns the payload value. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "CommonInventory|CommonItem", meta = (CustomStructureParam = "Value", ExpandBoolAsExecs = "ReturnValue", BlueprintInternalUseOnly = "true"))
	static ECommonInventoryResult GetPayloadValue(const FCommonItem& InCommonItem, int32& Value);

	/** Updates the payload value. */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "CommonInventory|CommonItem", meta = (CustomStructureParam = "Value", ExpandBoolAsExecs = "ReturnValue", BlueprintInternalUseOnly = "true"))
	static ECommonInventoryResult SetPayloadValue(UPARAM(Ref) FCommonItem& InCommonItem, const int32& Value);

	/** Invalidates data. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|CommonItem")
	static void Reset(UPARAM(Ref) FCommonItem& InCommonItem)
	{
		InCommonItem.Reset();
	}

	/** Resets payload to the default state from the registry. */
	UFUNCTION(BlueprintCallable, Category = "CommonInventory|CommonItem")
	static void ResetToDefault(UPARAM(Ref) FCommonItem& InCommonItem)
	{
		InCommonItem.ResetItem();
	}

	/** Returns a string version of the item. */
	UFUNCTION(BlueprintPure, Category = "CommonInventory|CommonItem", meta = (BlueprintAutocast))
	static FString ToString(const FCommonItem& InCommonItem)
	{
		return InCommonItem.ToString();
	}

private:

	DECLARE_FUNCTION(execGetPayloadValue);
	DECLARE_FUNCTION(execSetPayloadValue);

public: // FCommonInventoryView


};
