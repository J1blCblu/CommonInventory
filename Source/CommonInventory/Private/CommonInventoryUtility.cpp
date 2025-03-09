// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryUtility.h"

#include "CommonInventoryTypes.h"
#include "InventoryRegistry/CommonInventoryRegistryTypes.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/UnrealString.h"
#include "UObject/UObjectBaseUtility.h"
#include "UObject/UObjectThreadContext.h"

#if WITH_EDITOR
#include "CommonInventorySettings.h"
#include "Internationalization/Regex.h"
#include "Internationalization/Text.h"
#endif

FString CommonInventory::GetSerializedObjectPath()
{
	if (FUObjectSerializeContext* const SerializeContext = FUObjectThreadContext::Get().GetSerializeContext())
	{
		return GetPathNameSafe(SerializeContext->SerializedObject);
	}

	return FString(TEXT("None"));
}

bool CommonInventory::HasReferencers(FPrimaryAssetId InPrimaryAssetId, bool bRecursively, bool bUnloadedOnly)
{
	TArray<FAssetData> Referencers;
	bool bResult = GetReferencers(InPrimaryAssetId, Referencers, /* bRecursively */ false, bUnloadedOnly);

	if (!bResult && bRecursively)
	{
		FCommonInventoryRedirects::Get().TraversePermutations(InPrimaryAssetId, [&Referencers, &bResult, bUnloadedOnly](FPrimaryAssetId Permutation)
		{
			return (Referencers.Reset(), bResult = GetReferencers(Permutation, Referencers, /* bRecursively */ false, bUnloadedOnly)) == false;
		});
	}

	return bResult;
}

bool CommonInventory::GetReferencers(FPrimaryAssetId InPrimaryAssetId, TArray<FAssetData>& OutReferencers, bool bRecursively, bool bUnloadedOnly)
{
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	TArray<FAssetIdentifier> Referencers;

	AssetRegistry.GetReferencers(FCommonItem::MakeSearchableName(InPrimaryAssetId), Referencers, UE::AssetRegistry::EDependencyCategory::SearchableName);

	if (bRecursively)
	{
		FCommonInventoryRedirects::Get().TraversePermutations(InPrimaryAssetId, [&Referencers, &AssetRegistry](FPrimaryAssetId Permutation)
		{
			return (AssetRegistry.GetReferencers(FCommonItem::MakeSearchableName(Permutation), Referencers, UE::AssetRegistry::EDependencyCategory::SearchableName), true); // Continue.
		});
	}

	OutReferencers.Reserve(OutReferencers.Num() + Referencers.Num());

	for (const FAssetIdentifier& Referencer : Referencers)
	{
		AssetRegistry.GetAssetsByPackageName(Referencer.PackageName, OutReferencers);
	}

	if (bUnloadedOnly)
	{
		OutReferencers.RemoveAllSwap([](const FAssetData& InAssetData) { return InAssetData.IsAssetLoaded(); });
	}

	return !OutReferencers.IsEmpty();
}

#if WITH_EDITOR

bool CommonInventory::ValidateNamingConvention(const FString& InName, FText* OutErrorMessage /* = nullptr */)
{
	bool bResult = true;

	if (UCommonInventorySettings::Get()->bValidateNamingConvention)
	{
		if ((bResult = FRegexMatcher(FRegexPattern(UCommonInventorySettings::Get()->NamingConventionPattern), InName).FindNext()) == false && OutErrorMessage)
		{
			*OutErrorMessage = FText::Format(NSLOCTEXT("CommonInventoryEditor", "NamingConventionError", "The name {0} doesn't comply with the naming convention."), FText::FromString(InName));
		}
	}

	return bResult;
}

#endif
