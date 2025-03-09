// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryFunctionLibrary.h"

#include "InventoryRegistry/CommonInventoryRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryFunctionLibrary)

#define LOCTEXT_NAMESPACE "CommonInventoryFunctionLibrary"

bool UCommonInventoryFunctionLibrary::ContainsRecord(FPrimaryAssetId InPrimaryAssetId)
{
	return UCommonInventoryRegistry::Get().ContainsRecord(InPrimaryAssetId);
}

bool UCommonInventoryFunctionLibrary::ContainsArchetype(FPrimaryAssetType InArchetype)
{
	return UCommonInventoryRegistry::Get().ContainsArchetype(InArchetype);
}

void UCommonInventoryFunctionLibrary::GetRecordIds(TArray<FPrimaryAssetId>& OutPrimaryAssetIds, FPrimaryAssetType InArchetype)
{
	UCommonInventoryRegistry::Get().GetRecordIds(OutPrimaryAssetIds, InArchetype);
}

void UCommonInventoryFunctionLibrary::GetArchetypes(TArray<FPrimaryAssetType>& OutArchetypes)
{
	UCommonInventoryRegistry::Get().GetArchetypes(OutArchetypes);
}

ECommonInventoryResult UCommonInventoryFunctionLibrary::GetRegistrySharedData(FPrimaryAssetId InPrimaryAssetId, FCommonItemSharedData& OutSharedData)
{
	if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().GetRegistryRecord(InPrimaryAssetId))
	{
		OutSharedData = Record->SharedData;
		return ECommonInventoryResult::Valid;
	}

	return ECommonInventoryResult::NotValid;
}

ECommonInventoryResult UCommonInventoryFunctionLibrary::GetRegistryAssetPath(FPrimaryAssetId InPrimaryAssetId, FSoftObjectPath& OutAssetPath)
{
	if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().GetRegistryRecord(InPrimaryAssetId))
	{
		OutAssetPath = Record->AssetPath;
		return ECommonInventoryResult::Valid;
	}

	return ECommonInventoryResult::NotValid;
}

ECommonInventoryResult UCommonInventoryFunctionLibrary::GetRegistryDefaultPayload(FPrimaryAssetId InPrimaryAssetId, int32& Value)
{
	checkNoEntry(); return ECommonInventoryResult::NotValid;
}

DEFINE_FUNCTION(UCommonInventoryFunctionLibrary::execGetRegistryDefaultPayload)
{
	P_GET_STRUCT(FPrimaryAssetId, InPrimaryAssetId);

	// Read wildcard from FFrame.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* const Property = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* const PropertyValue = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ECommonInventoryResult Result = ECommonInventoryResult::NotValid;

	if (Property && PropertyValue)
	{
		P_NATIVE_BEGIN;

		if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().GetRegistryRecord(InPrimaryAssetId))
		{
			if (Record->DefaultPayload.GetScriptStruct()->IsChildOf(Property->Struct))
			{
				Property->Struct->CopyScriptStruct(PropertyValue, Record->DefaultPayload.GetMemory());
				Result = ECommonInventoryResult::Valid;
			}
		}

		P_NATIVE_END;
	}

	*static_cast<ECommonInventoryResult*>(RESULT_PARAM) = Result;
}

ECommonInventoryResult UCommonInventoryFunctionLibrary::GetRegistryCustomData(FPrimaryAssetId InPrimaryAssetId, int32& Value)
{
	checkNoEntry(); return ECommonInventoryResult::NotValid;
}

DEFINE_FUNCTION(UCommonInventoryFunctionLibrary::execGetRegistryCustomData)
{
	P_GET_STRUCT(FPrimaryAssetId, InPrimaryAssetId);

	// Read wildcard from FFrame.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* const Property = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* const PropertyValue = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ECommonInventoryResult Result = ECommonInventoryResult::NotValid;

	if (Property && PropertyValue)
	{
		P_NATIVE_BEGIN;

		if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().GetRegistryRecord(InPrimaryAssetId))
		{
			if (Record->CustomData.GetScriptStruct()->IsChildOf(Property->Struct))
			{
				Property->Struct->CopyScriptStruct(PropertyValue, Record->CustomData.GetMemory());
				Result = ECommonInventoryResult::Valid;
			}
		}

		P_NATIVE_END;
	}

	*static_cast<ECommonInventoryResult*>(RESULT_PARAM) = Result;
}

ECommonInventoryResult UCommonInventoryFunctionLibrary::GetPayloadValue(const FCommonItem& InCommonItem, int32& Value)
{
	checkNoEntry(); return ECommonInventoryResult::NotValid;
}

DEFINE_FUNCTION(UCommonInventoryFunctionLibrary::execGetPayloadValue)
{
	P_GET_STRUCT_REF(FCommonItem, InCommonItem);

	// Read wildcard from FFrame.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* const Property = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* const PropertyValue = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ECommonInventoryResult Result = ECommonInventoryResult::NotValid;

	if (Property && PropertyValue)
	{
		P_NATIVE_BEGIN;

		if (InCommonItem.HasPayload() && InCommonItem.GetPayload().GetScriptStruct()->IsChildOf(Property->Struct))
		{
			Property->Struct->CopyScriptStruct(PropertyValue, InCommonItem.GetPayload().GetMemory());
			Result = ECommonInventoryResult::Valid;
		}

		P_NATIVE_END;
	}

	*static_cast<ECommonInventoryResult*>(RESULT_PARAM) = Result;
}

ECommonInventoryResult UCommonInventoryFunctionLibrary::SetPayloadValue(FCommonItem& InCommonItem, const int32& Value)
{
	checkNoEntry(); return ECommonInventoryResult::NotValid;
}

DEFINE_FUNCTION(UCommonInventoryFunctionLibrary::execSetPayloadValue)
{
	P_GET_STRUCT_REF(FCommonItem, InCommonItem);

	// Read wildcard from FFrame.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	const FStructProperty* const Property = CastField<FStructProperty>(Stack.MostRecentProperty);
	void* const PropertyValue = Stack.MostRecentPropertyAddress;

	P_FINISH;

	ECommonInventoryResult Result = ECommonInventoryResult::NotValid;

	if (Property && PropertyValue)
	{
		P_NATIVE_BEGIN;
		
		if (InCommonItem.HasPayload() && InCommonItem.GetPayload().GetScriptStruct()->IsChildOf(Property->Struct))
		{
			Property->Struct->CopyScriptStruct(InCommonItem.GetMutablePayload().GetMutableMemory(), PropertyValue);
			Result = ECommonInventoryResult::Valid;
		}

		P_NATIVE_END;
	}

	*static_cast<ECommonInventoryResult*>(RESULT_PARAM) = Result;
}

#undef LOCTEXT_NAMESPACE
