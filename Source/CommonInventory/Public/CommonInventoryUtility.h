// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/PrimaryAssetId.h"
#include "VariadicStruct.h"

struct FAssetData;

class FString;
class FText;

namespace CommonInventory
{
	/** Returns the path name of a serialized object from the current serialization context. */
	COMMONINVENTORY_API FString GetSerializedObjectPath();

	/** Whether FPrimaryAssetId within FCommonItem is referenced. Recursive gathering requires type and name history depth permutations. */
	COMMONINVENTORY_API bool HasReferencers(FPrimaryAssetId InPrimaryAssetId, bool bRecursively = false, bool bUnloadedOnly = false);

	/** Returns all assets referencing FPrimaryAssetId within FCommonItem. Recursive gathering requires type and name history depth permutations. */
	COMMONINVENTORY_API bool GetReferencers(FPrimaryAssetId InPrimaryAssetId, TArray<FAssetData>& OutReferencers, bool bRecursively = false, bool bUnloadedOnly = false);

	/** Exports a generic script struct wrapper as text. */
	template<VariadicStruct::CScriptStructWrapper T>
	FString ExportScriptStruct(const T& InStructWrapper, bool bExportStructValue = true)
	{
		if (InStructWrapper.IsValid())
		{
			TStringBuilder<512> StringBuilder(InPlace, InStructWrapper.GetScriptStruct()->GetStructPathName());

			if (bExportStructValue)
			{
				FString StructValue;
				InStructWrapper.GetScriptStruct()->ExportText(StructValue, InStructWrapper.GetMemory(), InStructWrapper.GetMemory(), nullptr, PPF_None, nullptr);
				StringBuilder << ' '  << StructValue;
			}

			return FString(StringBuilder.Len(), StringBuilder.GetData());
		}

		return TEXT("None");
	}

#if WITH_EDITOR

	/** Conditionally validates the name against a pattern from the config. */
	COMMONINVENTORY_API bool ValidateNamingConvention(const FString& InName, FText* OutErrorMessage = nullptr);

#endif // WITH_EDITOR
}

/************************************************************************/
/* Privacy Leak Helper                                                  */
/************************************************************************/

// This template, is used to link an arbitrary class member, to the GetPrivate function.
template<typename Accessor, typename Accessor::MemberPointerType Member>
struct AccessPrivateHelper
{
	friend typename Accessor::MemberPointerType GetPrivate(Accessor InAccessor)
	{
		return Member;
	}
};

// Defines a class and template specialization, for a variable, needed for use with the INVENTORYCORE_GET_PRIVATE_MEMBER hook below.
#define COMMON_INVENTORY_IMPLEMENT_GET_PRIVATE_MEMBER(InClass, MemberName, VarType) \
struct InClass##MemberName##Accessor \
{ \
	using MemberPointerType = VarType InClass::*; \
	friend MemberPointerType GetPrivate(InClass##MemberName##Accessor); \
}; \
template struct AccessPrivateHelper<InClass##MemberName##Accessor, &InClass::MemberName>;

 // A macro for tidying up accessing of private members, through the above code.
#define COMMON_INVENTORY_GET_PRIVATE_MEMBER(InClass, InObj, MemberName) InObj.*GetPrivate(InClass##MemberName##Accessor())
