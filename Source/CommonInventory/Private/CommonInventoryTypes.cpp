// Copyright 2023 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryTypes.h"

#include "InventoryRegistry/CommonInventoryRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryTypes)

/************************************************************************/
/* FCommonItem                                                          */
/************************************************************************/

const FCommonItemSharedData* FCommonItem::GetSharedDataPtr() const
{
	if (PrimaryAssetId.IsValid())
	{
		if (const FCommonInventoryRegistryRecord* const RegistryRecord = UCommonInventoryRegistry::Get().GetRegistryRecord(PrimaryAssetId))
		{
			return &RegistryRecord->SharedData;
		}
	}

	return nullptr;
}

const FCommonItemSharedData& FCommonItem::GetSharedData() const
{
	return UCommonInventoryRegistry::Get().GetRegistryRecordChecked(PrimaryAssetId).SharedData;
}

FConstStructView FCommonItem::GetDefaultPayload() const
{
	if (PrimaryAssetId.IsValid())
	{
		return UCommonInventoryRegistry::Get().GetRegistryRecordChecked(PrimaryAssetId).DefaultPayload;
	}

	return FConstStructView();
}

FConstStructView FCommonItem::GetCustomData() const
{
	if (PrimaryAssetId.IsValid())
	{
		return UCommonInventoryRegistry::Get().GetRegistryRecordChecked(PrimaryAssetId).CustomData;
	}

	return FConstStructView();
}

FSoftObjectPath FCommonItem::GetAssetPath() const
{
	if (PrimaryAssetId.IsValid())
	{
		return UCommonInventoryRegistry::Get().GetRegistryRecordChecked(PrimaryAssetId).AssetPath;
	}

	return FSoftObjectPath();
}

void FCommonItem::ResetItem()
{
	if (PrimaryAssetId.IsValid())
	{
		if (UCommonInventoryRegistry::Get().ResetItem(PrimaryAssetId, Payload))
		{
			return;
		}

		PrimaryAssetId = FPrimaryAssetId();
	}

	Payload.Reset();
}

bool FCommonItem::ValidateItem() const
{
	if (PrimaryAssetId.IsValid())
	{
		return UCommonInventoryRegistry::Get().ValidateItem(PrimaryAssetId, Payload);
	}
	else
	{
		return !Payload.IsValid();
	}
}

void FCommonItem::SynchronizeItem()
{
	if (PrimaryAssetId.IsValid())
	{
		if (UCommonInventoryRegistry::Get().SynchronizeItem(PrimaryAssetId, Payload))
		{
			return;
		}

		PrimaryAssetId = FPrimaryAssetId();
	}

	Payload.Reset();
}

bool FCommonItem::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	// Mark the name as searchable, so we can later find out dependencies through the IAssetRegistry.
	if (Ar.IsSaving() && PrimaryAssetId.IsValid())
	{
		Ar.MarkSearchableName(FCommonItem::StaticStruct(), FName(PrimaryAssetId.ToString()));
	}
#endif

	if (UCommonInventoryRegistry* const Registry = UCommonInventoryRegistry::GetPtr())
	{
		// It's not thread safe to access SharedData as defaults.
		return Registry->SerializeItem(Ar, PrimaryAssetId, Payload);
	}
	else if ((Ar.IsObjectReferenceCollector() || Ar.IsSerializingDefaults()))
	{
		return Payload.Serialize(Ar << PrimaryAssetId);
	}

	return false;
}

bool FCommonItem::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	// UInventoryRegistry interface will set bOutSuccess and do logging for us.
	const UCommonInventoryRegistry& Registry = UCommonInventoryRegistry::Get();
	auto Context = Registry.NetSerializeItem(Ar, PrimaryAssetId, bOutSuccess);
	Registry.NetSerializeItemPayload(Ar, Payload, Context);

	return true;
}

/************************************************************************/
/* FCommonItemStack                                                     */
/************************************************************************/

bool FCommonItemStack::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	CommonItem.NetSerialize(Ar, nullptr, bOutSuccess);

	if (Ar.IsSaving())
	{
		bool bIsNegative = StackSize < 0;
		uint32 Absolute = bIsNegative ? -StackSize : StackSize;
		Ar.SerializeBits(&bIsNegative, 1);
		Ar.SerializeIntPacked(Absolute);
	}
	else
	{
		bool bIsNegative = false;
		uint32 Absolute = 0;
		Ar.SerializeBits(&bIsNegative, 1);
		Ar.SerializeIntPacked(Absolute);
		StackSize = bIsNegative ? -(int32)Absolute : Absolute;
	}

	return true;
}
