// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "AssetDefinition_CommonItemDefinition.h"
#include "CommonInventoryTypes.h"
#include "CommonItemDefinition.h"
#include "CommonInventoryEditorUtility.h"
#include "CommonItemDefinitionEditorStyle.h"
#include "InventoryRegistry/CommonInventoryRegistry.h"

// Menu Extension
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "Misc/DelayedAutoRegister.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetDefinition_CommonItemDefinition)

#define LOCTEXT_NAMESPACE "CommonInventoryEditor"

/************************************************************************/
/* MenuExtension_CommonItemDefinition                                   */
/************************************************************************/

namespace MenuExtension_CommonItemDefinition
{
	static void ExecuteFindReferences(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* const Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TArray<FAssetIdentifier> AssetIdentifiers;

			for (const FAssetData AssetData : Context->SelectedAssets)
			{
				const FPrimaryAssetId PrimaryAssetId = AssetData.GetPrimaryAssetId();

				if (UCommonInventoryRegistry::Get().ContainsRecord(PrimaryAssetId))
				{
					AssetIdentifiers.Emplace(FCommonItem::MakeSearchableName(PrimaryAssetId));

					FCommonInventoryRedirects::Get().TraversePermutations(PrimaryAssetId, [&AssetIdentifiers](FPrimaryAssetId Permutation)
					{
						AssetIdentifiers.Emplace(FCommonItem::MakeSearchableName(Permutation));
						return true; // Continue.
					});
				}
			}
			
			if (!AssetIdentifiers.IsEmpty())
			{
				FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
			}
		}
	}

	// https://herr-edgy.com/tutorials/extending-tool-menus-in-the-editor-via-c/
	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]
		{
			const FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

			if (UToolMenu* const Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UCommonItemDefinition::StaticClass()))
			{
				const TAttribute<FText> Label = LOCTEXT("ItemDefinition_FindReferences", "Find References");
				const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Search");

				FToolUIAction UIAction;
				UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteFindReferences);

				Menu->FindOrAddSection("GetAssetActions").AddMenuEntry("ItemDefinition_FindReferences", Label, TAttribute<FText>(), Icon, UIAction);
			}
		}));
	});
}

/************************************************************************/
/* UAssetDefinition_CommonItemDefinition                                */
/************************************************************************/

FText UAssetDefinition_CommonItemDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDefinition_CommonItemDefinition", "Common Item Defintion");
}

FLinearColor UAssetDefinition_CommonItemDefinition::GetAssetColor() const
{
	return FCommonItemDefinitionEditorStyle::TypeColor;
}

TSoftClassPtr<UObject> UAssetDefinition_CommonItemDefinition::GetAssetClass() const
{
	return UCommonItemDefinition::StaticClass();
}

FAssetSupportResponse UAssetDefinition_CommonItemDefinition::CanRename(const FAssetData& InAsset) const
{
	FText ErrorMsg;
	return CommonInventoryEditor::ValidateAssetName(InAsset, &ErrorMsg) ? FAssetSupportResponse::Supported() : FAssetSupportResponse::Error(ErrorMsg);
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_CommonItemDefinition::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { CommonInventoryEditor::GetAssetCategoryPath() };
	return Categories;
}

//EAssetCommandResult UAssetDefinition_CommonItemDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
//{
//	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
//
//	for (UCommonItemDefinition* Definition : OpenArgs.LoadObjects<UCommonItemDefinition>())
//	{
//		USmartObjectAssetEditor* AssetEditor = NewObject<USmartObjectAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
//		AssetEditor->SetObjectToEdit(Definition);
//		AssetEditor->Initialize();
//	}
//	return EAssetCommandResult::Handled;
//}

#undef LOCTEXT_NAMESPACE
