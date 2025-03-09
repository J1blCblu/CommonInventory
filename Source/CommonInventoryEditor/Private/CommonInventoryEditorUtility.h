// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"

struct FAssetCategoryPath;
struct FAssetData;

class FText;
class FString;
class UObject;

namespace CommonInventoryEditor
{
	// Returns custom asset category for the content browser.
	const FAssetCategoryPath& GetAssetCategoryPath();

	// @see implementation.
	bool IsNameAllowed(const FString& Name, FText* OutErrorMessage = nullptr);
	bool ValidateAssetName(const FAssetData& AssetData, FText* OutErrorMessage = nullptr);

	/** Tries to checkout the default config filename from the object. */
	bool CheckOutDefaultConfig(UObject* InObject);

	DECLARE_DELEGATE_RetVal(bool, FNotificationDelegate);

	/** Creates a stateless notification. */
	void MakeNotification(const FText& InText, bool bIsError = false, float InDuration = 10.f);

	/** Creates a notification with 'Decline' button and optional 'Confirm' button. */
	void MakeNotification(const FText& InText, const FText& InSubText, FNotificationDelegate OnConfirm = FNotificationDelegate(), float InDuration = 45.f);
};
