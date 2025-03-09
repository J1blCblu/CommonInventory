// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "AssetDefinition_CommonItemDefinition.generated.h"

UCLASS()
class UAssetDefinition_CommonItemDefinition : public UAssetDefinitionDefault
{
	GENERATED_BODY()

protected: //Overrides

	virtual FText GetAssetDisplayName() const override;
	virtual FLinearColor GetAssetColor() const override;
	virtual TSoftClassPtr<UObject> GetAssetClass() const override;
	//virtual FAssetSupportResponse CanDuplicate(const FAssetData& InAsset) const override;
	virtual FAssetSupportResponse CanRename(const FAssetData& InAsset) const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
	//virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};
