// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryEditorUtility.h"

#include "CommonInventorySettings.h"
#include "CommonInventoryUtility.h"
#include "InventoryRegistry/CommonInventoryRegistry.h"

#include "Algo/Find.h"
#include "AssetRegistry/AssetData.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Internationalization/Text.h"
#include "Misc/AssetCategoryPath.h"
#include "SourceControlHelpers.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "CommonInventoryEditor"

// Cached name during IsNameAllowed().
static FName AssetCachedNewName = NAME_None;

namespace CommonInventoryEditor
{
	const FAssetCategoryPath& GetAssetCategoryPath()
	{
		static const FAssetCategoryPath AssetCategoryPath(LOCTEXT("AssetCategory", "Common Inventory"));
		return AssetCategoryPath;
	}

	bool IsNameAllowed(const FString& Name, FText* OutErrorMessage)
	{
		AssetCachedNewName = FName(*Name);

		// Possible hooks to validate names:
		// IAssetTools::IsNameAllowed(FString); Validates a new name globally (files and folders).
		// UAssetDefinition::CanRename(FAssetData); Validates that an asset can actually be renamed, but the new name is not provided... at least for the actual rename.
		// FEditorDelegates::OnPreDestructiveAssetAction.Broadcast(OUT bool&); Same as above.
		// UFactory::FactoryCreateNew(FName); Return nullptr to skip asset creation.

		// UContentBrowserDataSource can be used to reflect files and directories with custom intent into the content browser.
		// UContentBrowserAssetDataSource is responsible for the Unreal asset ecosystem (UFS directories, .uasset, .umap).
		// UContentBrowserClassDataSource is responsible for native classes. Not very practical actually.
		// UContentBrowserFileDataSource is responsible for any other files.

		// UContentBrowserAssetDataSource::CanRenameItem()
		//	ContentBrowserAssetData::CanRenameAssetFileItem()
		//		AssetViewUtils::IsValidObjectPathForCreate()
		//			IAssetTools::IsNameAllowed(FString) <---- Cache new name
		//		IAssetTypeActions::CanRename(FAssetData)
		//			UAssetDefinition::CanRename(FAssetData) <---- Validate new name

		// UContentBrowserAssetDataSource::RenameItem()
		//	ContentBrowserAssetData::RenameItem()
		//		ContentBrowserAssetData::CanRenameAssetFileItem() <+>
		//		ContentBrowserAssetData::RenameAssetFileItem()
		//			FEditorDelegates::OnPreDestructiveAssetAction.Broadcast(OUT bool&)
		//			IAssetTools::RenameAssetsWithDialog()
		//				FAssetRenameManager::RenameAssetsWithDialog()

		// UContentBrowserAssetDataSource::OnNewAssetRequested(UFactory)
		// UContentBrowserAssetDataSource::OnBeginCreateAsset(FName, UClass, UFactory) <---- Interactive or with dialog
		//
		// Tick() <---- Interactive name typing
		//	UContentBrowserAssetDataSource::OnValidateItemName()
		//		UContentBrowserAssetDataSource::CanRenameItem()	<+>	
		//
		// UContentBrowserAssetDataSource::OnFinalizeCreateAsset() <---- Name commited
		//	IAssetTools::CreateAsset(FName, UClass, UFactory) return nullptr to fail

		// UContentBrowserAssetDataSource::DuplicateItem()
		//	ContentBrowserAssetData::DuplicateItem() <---- Doesn't duplicate the actual asset
		//
		// <=> <---- Type a new name and validate
		//
		// UContentBrowserAssetDataSource::OnFinalizeDuplicateAsset() <---- Name commited
		//	IAssetTools::DuplicateAsset(FName, UObject) return nullptr to fail

		return true;
	}

	bool ValidateAssetName(const FAssetData& AssetData, FText* OutErrorMessage /* = nullptr */)
	{
		const FName NewName = AssetCachedNewName;
		AssetCachedNewName = NAME_None;

		// NewName is None when displaying the asset context menu.
		if (!NewName.IsNone())
		{
			if (!CommonInventory::ValidateNamingConvention(NewName.ToString(), OutErrorMessage))
			{
				return false;
			}

			if (Algo::FindBy(UCommonInventoryRegistry::Get().GetRegistryRecords(), NewName, &FCommonInventoryRegistryRecord::GetPrimaryAssetName))
			{
				if (OutErrorMessage)
				{
					*OutErrorMessage = FText::Format(LOCTEXT("ReservedName", "The name {0} is reserved for existing data in the Inventory Registry."), FText::FromName(NewName));
				}

				return false;
			}

			// Check if the name is not reserved for a redirection.
			if (Algo::FindByPredicate(UCommonInventorySettings::Get()->PrimaryAssetNameRedirects, [NewName](const FCommonInventoryRedirector& Redirector) { return Redirector.HasValue(NewName); }))
			{
				const FPrimaryAssetId PrimaryAssetId = AssetData.GetPrimaryAssetId();

				// Allow the original asset to move back and forth in the redirection chain.
				if (!FCommonInventoryRedirects::CanResolveInto(UCommonInventorySettings::Get()->PrimaryAssetNameRedirects, NewName, PrimaryAssetId.PrimaryAssetName))
				{
					if (OutErrorMessage)
					{
						*OutErrorMessage = FText::Format(LOCTEXT("ReservedRedirectionName", "The name {0} is reserved for redirection into existing data in the Inventory Registry."), FText::FromName(NewName));
					}

					return false;
				}

				// Don't accept a name for not registered duplicates.
				if (const FCommonInventoryRegistryRecord* const RegistryRecord = UCommonInventoryRegistry::Get().GetRegistryRecord(PrimaryAssetId))
				{
					if (RegistryRecord->AssetPath != AssetData.GetSoftObjectPath())
					{
						if (OutErrorMessage)
						{
							*OutErrorMessage = FText::Format(LOCTEXT("RegistryReservedRedirectorName", "The name {0} is reserved for redirection into existing data of the same name in the Inventory Registry."), FText::FromName(NewName));
						}

						return false;
					}
				}
			}
		}

		return true;
	}

	bool CheckOutDefaultConfig(UObject* InObject)
	{
		return !USourceControlHelpers::IsEnabled() || USourceControlHelpers::CheckOutOrAddFile(InObject->GetDefaultConfigFilename());
	}

	void MakeNotification(const FText& InText, bool bIsError, float InDuration)
	{
		FNotificationInfo Notification = FNotificationInfo(InText);
		Notification.ExpireDuration = InDuration;
		Notification.FadeOutDuration = 3.f;
		Notification.bFireAndForget = true;

		if (TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Notification))
		{
			NotificationPtr->SetCompletionState(bIsError ? SNotificationItem::CS_Fail : SNotificationItem::CS_Success);
		}
	}

	void MakeNotification(const FText& InText, const FText& InSubText, FNotificationDelegate OnConfirm, float InDuration)
	{
		struct FReactiveNotification
		{
			void OnButtonPressed(FNotificationDelegate InDelegate)
			{
				SNotificationItem::ECompletionState Result = SNotificationItem::CS_Fail;

				if (InDelegate.Execute())
				{
					Result = SNotificationItem::CS_Success;
				}

				TSharedPtr<SNotificationItem> NotificationSP = Notification.Pin();
				NotificationSP->SetCompletionState(Result);
				NotificationSP->Fadeout();
			}

			void OnDecline(TSharedRef<FReactiveNotification> NotificationHolder)
			{
				TSharedPtr<SNotificationItem> NotificationSP = Notification.Pin();
				NotificationSP->SetCompletionState(SNotificationItem::CS_Success); // Success will hide buttons.
				NotificationSP->Fadeout();
			}

			// Owning notification.
			TWeakPtr<SNotificationItem> Notification;
		};

		TSharedRef<FReactiveNotification> PendingNotification = MakeShared<FReactiveNotification>();

		FNotificationInfo Notification = FNotificationInfo(InText);
		Notification.SubText = InSubText;
		Notification.ExpireDuration = InDuration;
		Notification.FadeOutDuration = 3.f;
		Notification.bFireAndForget = true;
		Notification.WidthOverride = 400;

		if (OnConfirm.IsBound())
		{
			Notification.ButtonDetails.Emplace(
				LOCTEXT("NotificationConfirm", "Confirm"),
				FText::GetEmpty(),
				FSimpleDelegate::CreateSP(PendingNotification, &FReactiveNotification::OnButtonPressed, MoveTemp(OnConfirm)),
				SNotificationItem::CS_None);
		}

		// Decline button will keep PendingNotification.
		Notification.ButtonDetails.Emplace(
			LOCTEXT("NotificationDecline", "Decline"),
			FText::GetEmpty(),
			FSimpleDelegate::CreateSP(PendingNotification, &FReactiveNotification::OnDecline, PendingNotification),
			SNotificationItem::CS_None);

		PendingNotification->Notification = FSlateNotificationManager::Get().AddNotification(Notification);
	}
}
