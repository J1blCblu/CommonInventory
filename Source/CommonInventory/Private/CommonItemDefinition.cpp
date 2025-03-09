// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonItemDefinition.h"

#if WITH_EDITOR
#include "CommonInventoryUtility.h"
#include "InventoryRegistry/CommonInventoryRegistry.h"
#include "InventoryRegistry/DataSources/AssetManagerDataSource.h"

#include "Misc/DataValidation.h"
#include "Internationalization/Text.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonItemDefinition)

void UCommonItemDefinition::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
	{
		SharedData.PrimaryAssetId = GetPrimaryAssetId();
	}
}

#if WITH_EDITOR

void UCommonItemDefinition::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	Super::PreSave(ObjectSaveContext);

	if (!ObjectSaveContext.IsProceduralSave())
	{
		SharedData.PrimaryAssetId = GetPrimaryAssetId();

		if (UAssetManagerDataSource* const DataSource = Cast<UAssetManagerDataSource>(UCommonInventoryRegistry::Get().GetDataSource()))
		{
			DataSource->RefreshItemDefinition(this);
		}
	}
}

#define LOCTEXT_NAMESPACE "CommonInventoryEditor"

EDataValidationResult UCommonItemDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;
	const FString PackageName = GetOutermost()->GetName();

	// Validate PrimaryAssetName.
	if (GetPrimaryAssetId().PrimaryAssetName != FPackageName::GetShortFName(PackageName))
	{
		Context.AddError(FText::Format(LOCTEXT("PrimaryAssetNameMismatch", "PrimaryAssetName must match the asset name for {0}."), FText::FromString(PackageName)));
		Result = EDataValidationResult::Invalid;
	}

	// Validate naming convention.
	if (FText ErrorMessage; !CommonInventory::ValidateNamingConvention(FPackageName::GetShortName(PackageName), &ErrorMessage))
	{
		Context.AddError(MoveTemp(ErrorMessage));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#endif // WITH_EDITOR
