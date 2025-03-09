// Copyright 2023 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventorySettings.h"

#include "InventoryRegistry/CommonInventoryRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventorySettings)

#if WITH_EDITOR

void UCommonInventorySettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	FCommonInventoryRedirects::Get().ForceRefresh();
}

void UCommonInventorySettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (const FProperty* const Property = PropertyChangedEvent.Property)
	{
		if (UCommonInventoryRegistry* const Registry = UCommonInventoryRegistry::GetPtr())
		{
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCommonInventorySettings, PrimaryAssetNameRedirects)
				|| Property->GetFName() == GET_MEMBER_NAME_CHECKED(UCommonInventorySettings, PrimaryAssetTypeRedirects))
			{
				FCommonInventoryRedirects::Get().ForceRefresh();
			}
		}
	}

	// Broadcast SettingsChangedDelegate after changes.
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif
