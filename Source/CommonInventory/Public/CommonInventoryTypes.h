// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetIdentifier.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "GameplayTagContainer.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CoreMiscDefines.h"
#include "Misc/StringBuilder.h"
#include "UObject/Class.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/PropertyPortFlags.h"
#include "VariadicStruct.h"

#include "CommonInventoryTypes.generated.h"

class FArchive;
class UPackageMap;

struct FAssetIdentifier;
struct FCommonItemSharedData;
struct FSoftObjectPath;

FArchive& operator<<(FArchive& Ar, FCommonItem& InItem);
FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FCommonItem& InItem);

/**
 * A lightweight dynamic instance of an item with an optional payload.
 * The payload is synchronized with the default payload from the registry.
 * The recommended payload size is up to 24 bytes, which avoids dynamic allocation.
 * 
 * UPROPERTY meta params:
 * 1. AllowedArchetypes - Filters items in the selector by a list of archetypes.
 * 2. DisallowedArchetypes - Excludes items from the selector by a list of archetypes.
 *
 * @Note: The lifetime of the type is bound by the lifetime of UEngineSubsystem (UInventoryRegistry).
 */
USTRUCT(BlueprintType, meta = (DisableSplitPin, HasNativeMake = "/Script/CommonInventory.CommonInventoryFunctionLibrary.MakeCommonItem"))
struct FCommonItem
{
	GENERATED_BODY()

	//@TODO: Implement Iris.NetSerializer. Refs: GameplayTagNetSerializer, InstancedStructNetSerializer.
	//@TODO: Maybe replace FPrimaryAssetId/FPrimaryAssetType with FCommonItemId/FCommonItemArchetype.
	//@TODO: Add pin customization for MakeCommonItem(FPrimaryAssetId).

	FCommonItem() = default;

	/** Constructor from FPrimaryAssetId. */
	explicit FCommonItem(FPrimaryAssetId InPrimaryAssetId)
		: PrimaryAssetId(InPrimaryAssetId)
	{
		ResetItem();
	}

	/** Can be used to defer payload init. */
	static inline constexpr struct FDeferPayloadInit final {} DeferPayloadInit;

	/** Constructor from FPrimaryAssetId without payload init. Can be used in contexts where the payload would be excessive. */
	explicit FCommonItem(FPrimaryAssetId InPrimaryAssetId, FDeferPayloadInit)
		: PrimaryAssetId(InPrimaryAssetId)
	{
		// Do nothing.
	}

	/** Generic constructor from FPrimaryAssetId and payload value. The payload type must match the default payload type. */
	template<VariadicStruct::CSupportedType T>
	FCommonItem(FPrimaryAssetId InPrimaryAssetId, T&& InPayloadValue)
		: PrimaryAssetId(InPrimaryAssetId), Payload(FVariadicStruct::Make(Forward<T>(InPayloadValue)))
	{
		checkf(ValidateItem(), TEXT("FCommonItem: Constructor payload type mismatch."));
	}

	/** Payload sync isn't required. */
	FCommonItem(FCommonItem&&) = default;
	FCommonItem(const FCommonItem&) = default;
	FCommonItem& operator=(FCommonItem&&) = default;
	FCommonItem& operator=(const FCommonItem&) = default;

	/** Comparison. */
	bool operator==(const FCommonItem& Other) const { return PrimaryAssetId == Other.PrimaryAssetId && Payload.Identical(&Other.Payload, PPF_None); }
	bool operator!=(const FCommonItem& Other) const { return !operator==(Other); }

	friend class FCommonItemCustomization;
	friend class FCommonItemPayloadBuilder;

	/** Constructs a searchable name from type and name that can be used to find out dependencies through the IAssetRegistry. */
	static FAssetIdentifier MakeSearchableName(FPrimaryAssetId InPrimaryAssetId)
	{
		return FAssetIdentifier(FCommonItem::StaticStruct(), FName(InPrimaryAssetId.ToString()));
	}

public: // Data

	/** Whether the item is valid. */
	bool IsValid() const { return PrimaryAssetId.IsValid(); }

	/** Returns FPrimaryAssetId of the item. */
	FPrimaryAssetId GetPrimaryAssetId() const {	return PrimaryAssetId; }

	/** Updates FPrimaryAssetId of the item and resets payload. */
	void SetPrimaryAssetId(FPrimaryAssetId InPrimaryAssetId)
	{
		if (PrimaryAssetId != InPrimaryAssetId)
		{
			PrimaryAssetId = InPrimaryAssetId;
			ResetItem();
		}
	}

	/** Whether the item contains a valid payload object. */
	bool HasPayload() const { return Payload.IsValid(); }

	/** Returns the payload value pointer. */
	template<typename T>
	const T* GetPayloadValuePtr() const { return Payload.GetValuePtr<T>(); }

	/** Returns the payload value. */
	template<typename T>
	const T& GetPayloadValue() const { return Payload.GetValue<T>(); }

	/** Return the mutable payload value pointer. */
	template<typename T>
	T* GetMutablePayloadValuePtr() { return Payload.GetMutableValuePtr<T>(); }

	/** Returns the mutable payload value. */
	template<typename T>
	T& GetMutablePayloadValue() { return Payload.GetMutableValue<T>(); }

	/** Returns payload wrapper. */
	const FVariadicStruct& GetPayload() const { return Payload; }

	/** Returns mutable payload wrapper. @Note: Type update may result in UB. */
	FVariadicStruct& GetMutablePayload() { return Payload; }

public: // Shared Data

	/** [[Not Thread Safe]] Returns shared data pointer from the registry. */
	COMMONINVENTORY_API const FCommonItemSharedData* GetSharedDataPtr() const;

	/** [[Not Thread Safe]] Returns shared data from the registry. */
	COMMONINVENTORY_API const FCommonItemSharedData& GetSharedData() const;

	/** [[Not Thread Safe]] Returns default payload from the registry. */
	COMMONINVENTORY_API FConstStructView GetDefaultPayload() const;

	/** [[Not Thread Safe]] Returns custom data from the registry. */
	COMMONINVENTORY_API FConstStructView GetCustomData() const;

	/** [[Not Thread Safe]] Returns asset path from the registry. */
	COMMONINVENTORY_API FSoftObjectPath GetAssetPath() const;

public: // Utility

	/** Resets payload to the default state from the registry. */
	COMMONINVENTORY_API void ResetItem();

	/** Whether FPrimaryAssetId is synchronized with the payload from the registry. */
	COMMONINVENTORY_API bool ValidateItem() const;

	/** Redirects FPrimaryAssetId and synchronizes with the default payload from the registry. */
	COMMONINVENTORY_API void SynchronizeItem();

	/** Invalidates data. */
	void Reset()
	{
		PrimaryAssetId = FPrimaryAssetId();
		Payload.Reset();
	}

	/** Returns a string version of the item. */
	FString ToString() const
	{
		const TStringBuilder<256> Builder(InPlace, *this);
		return FString(Builder.Len(), Builder.GetData());
	}

	/** Returns the searchable name of the item. */
	FAssetIdentifier GetSearchableName() const
	{
		return MakeSearchableName(PrimaryAssetId);
	}

public: // StructOpsTypeTraits

	COMMONINVENTORY_API bool Serialize(FArchive& Ar);
	COMMONINVENTORY_API bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess);

private:

	/** Optional payload object synchronized with the default payload from the registry. */
	UPROPERTY(EditAnywhere, Category = "CommonItem")
	FVariadicStruct Payload;

	/** Type and name of the item. */
	UPROPERTY(EditAnywhere, Category = "CommonItem")
	FPrimaryAssetId PrimaryAssetId;
};

template<>
struct TStructOpsTypeTraits<FCommonItem> : public TStructOpsTypeTraitsBase2<FCommonItem>
{
	enum
	{
		WithCopy = true,
		WithIdenticalViaEquality = true,
		WithSerializer = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true, // By definition, can't contain data per connection.
	};

	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

inline FArchive& operator<<(FArchive& Ar, FCommonItem& InItem)
{
	InItem.Serialize(Ar);
	return Ar;
}

inline FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FCommonItem& InItem)
{
	Builder << InItem.GetPrimaryAssetId();

	if (InItem.HasPayload())
	{
		if (FString PayloadStr; InItem.GetPayload().ExportTextItem(PayloadStr))
		{
			Builder << ' ' << PayloadStr;
		}
	}

	return Builder;
}

/**
 * A struct wrapper over a group of FCommonItem's.
 * Doesn't imply the stack size restrictions from the defaults.
 * Instead, the higher level code should handle that.
 */
USTRUCT(BlueprintType)
struct FCommonItemStack
{
	GENERATED_BODY()

	FCommonItemStack() = default;

	bool operator==(const FCommonItemStack&) const = default;
	bool operator!=(const FCommonItemStack&) const = default;

public:

	/** The actual item info. */
	UPROPERTY(EditAnywhere, Category = "Item", meta = (ShowOnlyInnerProperties))
	FCommonItem CommonItem;

	/** The amount of grouped items. */
	UPROPERTY(EditAnywhere, Category = "Item", meta = (Units = "x", NoSpinbox = true))
	int32 StackSize = 1;

public: // Utility

	/** Returns a string version of the item. */
	FString ToString() const
	{
		const TStringBuilder<256> Builder(InPlace, *this);
		return FString(Builder.Len(), Builder.GetData());
	}

public: // StructOpsTypeTraits

	COMMONINVENTORY_API bool NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FCommonItemStack> : public TStructOpsTypeTraitsBase2<FCommonItemStack>
{
	enum
	{
		WithIdenticalViaEquality = true,
		WithNetSerializer = true,
		WithNetSharedSerialization = true, // By definition, can't contain data per connection.
	};
};

inline FStringBuilderBase& operator<<(FStringBuilderBase& Builder, const FCommonItemStack& InItemStack)
{
	return Builder << 'x' << InItemStack.StackSize << ' ' << InItemStack.CommonItem;
}

/**
 * The hot data shared between all items of the same type identified by FPrimaryAssetId.
 * Designed to be lightweight and intended only for the core functionality to avoid runtime overhead.
 */
USTRUCT(BlueprintType)
struct FCommonItemSharedData
{
	GENERATED_BODY()

	FCommonItemSharedData() = default;

	bool operator==(const FCommonItemSharedData&) const = default;
	bool operator!=(const FCommonItemSharedData&) const = default;

public:

	/** UCommonItemDefinition the data copied from. */
	UPROPERTY(BlueprintReadOnly, Category = "SharedData")
	FPrimaryAssetId PrimaryAssetId;

	/** List of customizable gameplay tags. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SharedData")
	FGameplayTagContainer GameplayTags;

	/** The maximum amount of grouped items of the same type. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SharedData", meta = (Units = "x", NoSpinbox = true))
	int32 MaxStackSize = 64;
};

template<>
struct TStructOpsTypeTraits<FCommonItemSharedData> : public TStructOpsTypeTraitsBase2<FCommonItemSharedData>
{
	enum
	{
		WithCopy = true,
		WithIdenticalViaEquality = true,
	};

	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};

namespace CommonInventory
{
	/** Special archetype for implementing internal items. */
	inline const FPrimaryAssetType SpecialArchetype = FPrimaryAssetType("CommonInventoryArchetype");
}

/**
 * Special internal item for handling redirects at runtime, which might be useful for spatial items.
 *
 * @Note: Spatial items could be handled in the following way:
 * 1. The anchor item should be the first element with the lowest index.
 * 2. All possible offsets could be encoded as a flat bitmap relative to the anchor.
 *    For example, uint32 could be encoded as follows for two-dimensional inventories:
 * 
 *		0 1 2 3 4 5 6 7 8
 *		1 A * * * * * * *
 *		2 R R * * * * * *
 *		3 R R R * * * * *
 *		4 R R R R * * * *
 */
USTRUCT(meta = (Hidden))
struct FCommonInventoryRedirectorItem
{
	GENERATED_BODY();

	// @TODO: Register special items in the registry state.

	FCommonInventoryRedirectorItem() = default;

	bool operator==(const FCommonInventoryRedirectorItem&) const = default;
	bool operator!=(const FCommonInventoryRedirectorItem&) const = default;

public:

	/** Reserved item name. */
	static inline const FName ItemName = FName("Redirector");

	/** Returns FPrimaryAssetId. */
	static FPrimaryAssetId GetPrimaryAssetId() { return { CommonInventory::SpecialArchetype, ItemName }; }

public:

	/** Bitmap index for calculating the anchor index. */
	UPROPERTY()
	uint8 BitMapIndex;
};

template<>
struct TStructOpsTypeTraits<FCommonInventoryRedirectorItem> : public TStructOpsTypeTraitsBase2<FCommonInventoryRedirectorItem>
{
	enum
	{
		WithZeroConstructor = true,
		WithNoDestructor = true,
		WithIdenticalViaEquality = true,
		WithNetSharedSerialization = true,
	};

	static constexpr EPropertyObjectReferenceType WithSerializerObjectReferences = EPropertyObjectReferenceType::None;
};
