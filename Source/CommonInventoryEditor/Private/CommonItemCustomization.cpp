// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonItemCustomization.h"
#include "CommonInventoryTypes.h"
#include "CommonInventoryUtility.h"
#include "InventoryRegistry/DataSources/AssetManagerDataSource.h"
#include "InventoryRegistry/CommonInventoryRegistry.h"

#include "Algo/Transform.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Engine/AssetManager.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "IStructureDataProvider.h"
#include "PropertyBagDetails.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "StructUtilsDelegates.h"

#include "Styling/SlateIconFinder.h"
#include "SSearchableComboBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SToolTip.h"

#define LOCTEXT_NAMESPACE "CommonInventoryEditor"

// Signature: (const UScriptStruct* InScriptStruct, uint8* InMemory, const uint8* InDefaults, UPackage* InPackage).
static void EnumeratePayloads(TSharedPtr<IPropertyHandle> InStructProperty, TFunctionRef<bool(const UScriptStruct*, uint8*, const uint8*, UPackage*)> InFunc)
{
	if (InStructProperty.IsValid())
	{
		TArray<UPackage*> Packages;
		InStructProperty->GetOuterPackages(Packages);
		InStructProperty->EnumerateRawData([&](void* RawData, const int32 DataIndex, const int32 /* NumData */)
		{
			if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData); CommonItem && CommonItem->HasPayload())
			{
				const UScriptStruct* const ScriptStruct = CommonItem->GetPayload().GetScriptStruct();
				const FConstStructView DefaultPayload = CommonItem->GetDefaultPayload();
				checkf(ScriptStruct == DefaultPayload.GetScriptStruct(), TEXT("FCommonItem: Payload type mismatch."));

				uint8* const Memory = CommonItem->GetMutablePayload().GetMutableMemory();
				const uint8* const Defaults = DefaultPayload.GetMemory();
				UPackage* Package = nullptr;

				if (ensure(Packages.IsValidIndex(DataIndex)))
				{
					Package = Packages[DataIndex];
				}

				return InFunc(ScriptStruct, Memory, Defaults, Package);
			}

			return true; // Continue.
		});
	}
}

/************************************************************************/
/* FCommonItemCustomization                                             */
/************************************************************************/

// The delegate is mainly used to refresh the details for instances after editing the archetype.
static FSimpleMulticastDelegate ForceRefreshDetailsDelegate;

FCommonItemCustomization::~FCommonItemCustomization()
{
	if (UCommonInventoryRegistry* const Registry = UCommonInventoryRegistry::GetPtr())
	{
		Registry->Unregister_OnPostRefresh(OnRegistryRefreshedHandle);
	}

	FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
	ForceRefreshDetailsDelegate.Remove(OnForceRefreshDetailsHandle);
}

void FCommonItemCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
{
	StructProperty = InStructPropertyHandle;
	PropertyUtilities = InStructCustomizationUtils.GetPropertyUtilities();
	PrimaryAssetIdChildProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCommonItem, PrimaryAssetId));
	PayloadChildProperty = StructProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCommonItem, Payload));
	check(PrimaryAssetIdChildProperty && PayloadChildProperty && PropertyUtilities);

	TArray<FPrimaryAssetType> AllowedArchetypes, DisallowedArchetypes;
	ParseMetadata(AllowedArchetypes, DisallowedArchetypes);

	InHeaderRow
	.NameContent()
	[
		StructProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(320.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot() // Selector Button
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SCommonInventoryPrimaryAssetNameSelector)
			.AllowedArchetypes(MoveTemp(AllowedArchetypes))
			.DisallowedArchetypes(MoveTemp(DisallowedArchetypes))
			.DisplayValue(this, &FCommonItemCustomization::GetDisplayValueAsString)
			.IsEditable(StructProperty.ToSharedRef(), &IPropertyHandle::IsEditable)
			.OnSelectionChanged(TDelegate<void(FPrimaryAssetId)>::CreateSP(this, &FCommonItemCustomization::OnPrimaryAssetIdSelected))
		]
		+ SHorizontalBox::Slot() // Clear Button
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FCommonItemCustomization::OnClear))
		]
		+ SHorizontalBox::Slot() // Browse Asset Button
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FCommonItemCustomization::OnBrowseTo))
		]
		+ SHorizontalBox::Slot() // Edit Asset Button
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.WidthOverride(22.0f)
			.HeightOverride(22.0f)
			.ToolTipText(LOCTEXT("EditAssetsDesc", "Open Asset Editor"))
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FCommonItemCustomization::OnEditAsset)
				.ContentPadding(0.0f)
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Edit"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]
		+ SHorizontalBox::Slot() // Find References Button
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.WidthOverride(22.0f)
			.HeightOverride(22.0f)
			.ToolTipText(LOCTEXT("FindReferencesDesc", "Find References"))
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(this, &FCommonItemCustomization::OnFindReferencers)
				.ContentPadding(0.0f)
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Search"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]
	];

	//OnForceRefreshDetailsHandle = ForceRefreshDetailsDelegate.AddSP(PayloadChildProperty.ToSharedRef(), &IPropertyHandle::RequestRebuildChildren);
	OnForceRefreshDetailsHandle = ForceRefreshDetailsDelegate.AddSP(PropertyUtilities.ToSharedRef(), &IPropertyUtilities::RequestForceRefresh);

	// We need to manually rebuild the payload tree since undo/redo doesn't trigger OnPropertyValueChanged().
	FEditorDelegates::PostUndoRedo.AddSP(PropertyUtilities.ToSharedRef(), &IPropertyUtilities::RequestForceRefresh);

	// Hard reset payload if FPrimaryAssetId has changed. 
	PrimaryAssetIdChildProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCommonItemCustomization::OnResetPayload));

	// Handle UDS reinstancing.
	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddSP(this, &FCommonItemCustomization::OnObjectsReinstanced);

	// Refresh the details on registry updates.
	OnRegistryRefreshedHandle = UCommonInventoryRegistry::Get().Register_OnPostRefresh(FSimpleMulticastDelegate::FDelegate::CreateSP(PropertyUtilities.ToSharedRef(), &IPropertyUtilities::RequestForceRefresh));
}

void FCommonItemCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils)
{
	InStructBuilder.AddCustomBuilder(MakeShared<FCommonItemPayloadBuilder>(StructProperty, PayloadChildProperty));
}

void FCommonItemCustomization::ParseMetadata(TArray<FPrimaryAssetType>& OutAllowedArchetypes, TArray<FPrimaryAssetType>& OutDisallowedArchetypes)
{
	static const FName AllowedArchetypesMeta = TEXT("AllowedArchetypes");
	static const FName DisallowedArchetypesMeta = TEXT("DisallowedArchetypes");

	if (StructProperty.IsValid())
	{
		auto CollectArchetypes = [this](FName MetaParam, TArray<FPrimaryAssetType>& OutArhetypes)
			{
				if (const FString& FilterTypeStr = StructProperty->GetMetaData(MetaParam); !FilterTypeStr.IsEmpty())
				{
					TArray<FString> AssetTypes;
					FilterTypeStr.ParseIntoArray(AssetTypes, TEXT(","), /* InCullEmpty */ true);
					Algo::Transform(AssetTypes, OutArhetypes, &FString::operator*);
				}
			};

		CollectArchetypes(AllowedArchetypesMeta, OutAllowedArchetypes);
		CollectArchetypes(DisallowedArchetypesMeta, OutDisallowedArchetypes);
	}
}

void FCommonItemCustomization::OnResetPayload()
{
#if 0
	// This doesn't propagate changes to instances from the archetype.
	StructProperty->EnumerateRawData([](void* RawData, const int32 /* DataIndex */, const int32 /* NumData */)
	{
		if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData))
		{
			CommonItem->ResetItem();
		}

		return true; // Continue.
	});

	PropertyUtilities->RequestForceRefresh();
#endif

	if (StructProperty.IsValid() && PayloadChildProperty.IsValid())
	{
		if (FProperty* const PayloadProperty = PayloadChildProperty->GetProperty())
		{
			PayloadChildProperty->NotifyPreChange();

			TArray<UObject*> OuterObjects;
			PayloadChildProperty->GetOuterObjects(OuterObjects);

			TArray<FString> DefaultPayloads;
			StructProperty->EnumerateRawData([&DefaultPayloads, &OuterObjects, PayloadProperty](void* RawData, const int32 DataIndex, const int32 /* NumData */)
			{
				// Don't reset the original value, just export its defaults.
				if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData))
				{
					const FVariadicStruct DefaultPayload = FVariadicStruct::Make(CommonItem->GetDefaultPayload());
					PayloadProperty->ExportText_Direct(DefaultPayloads.AddDefaulted_GetRef(), &DefaultPayload, nullptr, OuterObjects[DataIndex], PPF_ForDiff);
				}

				return true; // Continue.
			});

			// This will update the archetype and correctly propagate changes across instances.
			PayloadChildProperty->SetPerObjectValues(DefaultPayloads, EPropertyValueSetFlags::DefaultFlags);
			PayloadChildProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
			PayloadChildProperty->NotifyFinishedChangingProperties();

			ForceRefreshDetailsDelegate.Broadcast();
		}
	}
}

void FCommonItemCustomization::OnObjectsReinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& ObjectMap)
{
	// Force update the details when BP is compiled, since we may cached hold references to the old object or class.
	if (!ObjectMap.IsEmpty()) PropertyUtilities->RequestForceRefresh();
}

void FCommonItemCustomization::OnPrimaryAssetIdSelected(FPrimaryAssetId PrimaryAssetId)
{
	if (PrimaryAssetIdChildProperty.IsValid() && PrimaryAssetIdChildProperty->IsValidHandle())
	{
		if (PrimaryAssetId.IsValid())
		{
			PrimaryAssetIdChildProperty->SetValueFromFormattedString(PrimaryAssetId.ToString());
		}
		else // SetValueFromFormattedString() doesn't handle empty strings...
		{
			TArray<FString> Values;
			Values.AddDefaulted(PrimaryAssetIdChildProperty->GetNumPerObjectValues());
			PrimaryAssetIdChildProperty->SetPerObjectValues(Values);
		}
	}
}

FString FCommonItemCustomization::GetDisplayValueAsString() const
{
	FString ResultValue;

	if (PrimaryAssetIdChildProperty.IsValid() && PrimaryAssetIdChildProperty->IsValidHandle())
	{
		FPropertyAccess::Result Result = PrimaryAssetIdChildProperty->GetValueAsDisplayString(ResultValue);

		if (ResultValue.IsEmpty())
		{
			ResultValue = (PrimaryAssetIdChildProperty->GetPerObjectValue(0, ResultValue), ResultValue.IsEmpty()) ? TEXT("None") : TEXT("Multiple Values");
		}
		else if (Result != FPropertyAccess::MultipleValues)
		{
			// Just display the name without the type prefix.
			if (const int32 Idx = ResultValue.Find(TEXT(":")); Idx != INDEX_NONE)
			{
				ResultValue.RightChopInline(Idx + 1, false);
			}
		}
	}

	return ResultValue;
}

void FCommonItemCustomization::OnClear()
{
	if (StructProperty->IsEditable())
	{
		OnPrimaryAssetIdSelected(FPrimaryAssetId());
	}
}

void FCommonItemCustomization::OnBrowseTo()
{
	if (StructProperty.IsValid() && StructProperty->IsValidHandle() && GEditor)
	{
		TArray<FAssetData> AssetDataArray;
		AssetDataArray.Reserve(StructProperty->GetNumPerObjectValues());

		StructProperty->EnumerateRawData([&AssetDataArray](void* RawData, const int32 /* DataIndex */, const int32 /* NumData */)
		{
			if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData); CommonItem && CommonItem->IsValid())
			{
				if (FAssetData AssetData; UAssetManager::Get().GetAssetDataForPath(CommonItem->GetAssetPath(), AssetData))
				{
					AssetDataArray.Emplace(MoveTemp(AssetData));
				}
			}

			return true; // Continue.
		});

		if (!AssetDataArray.IsEmpty())
		{
			GEditor->SyncBrowserToObjects(AssetDataArray);
		}
	}
}

FReply FCommonItemCustomization::OnEditAsset()
{
	if (StructProperty.IsValid() && StructProperty->IsValidHandle() && GEditor)
	{
		StructProperty->EnumerateRawData([](void* RawData, const int32 /* DataIndex */, const int32 /* NumData */)
		{
			if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData); CommonItem && CommonItem->IsValid())
			{
				GEditor->EditObject(CommonItem->GetAssetPath().ResolveObject());
			}

			return true; // Continue.
		});
	}

	return FReply::Handled();
}

FReply FCommonItemCustomization::OnFindReferencers()
{
	if (StructProperty.IsValid() && StructProperty->IsValidHandle() && FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Reserve(StructProperty->GetNumPerObjectValues());

		StructProperty->EnumerateRawData([&AssetIdentifiers](void* RawData, const int32 /* DataIndex */, const int32 /* NumData */)
		{
			if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData); CommonItem && CommonItem->IsValid())
			{
				AssetIdentifiers.Emplace(CommonItem->GetSearchableName());

				FCommonInventoryRedirects::Get().TraversePermutations(CommonItem->GetPrimaryAssetId(), [&AssetIdentifiers](FPrimaryAssetId Permutation)
				{
					AssetIdentifiers.Emplace(FCommonItem::MakeSearchableName(Permutation));
					return true; // Continue.
				});
			}

			return true; // Continue.
		});

		if (!AssetIdentifiers.IsEmpty())
		{
			FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
		}
	}

	return FReply::Handled();
}

/************************************************************************/
/* FCommonItemPayloadProvider                                           */
/************************************************************************/

/**
 * Provides Payload data for a property node from FCommonItem.
 */
class FCommonItemPayloadProvider : public IStructureDataProvider
{
	TSharedPtr<IPropertyHandle> StructProperty;

public:

	explicit FCommonItemPayloadProvider(TSharedPtr<IPropertyHandle> InStructProperty)
		: StructProperty(InStructProperty)
	{
		check(StructProperty.IsValid());
	}

	void Reset()
	{
		StructProperty.Reset();
	}

public: // Overrides

	virtual bool IsPropertyIndirection() const override
	{
		return true; // We have double indirection: FCommonItem -> (FVariadicStruct) Payload -> Memory.
	}

	virtual bool IsValid() const override
	{
		bool bHasValidData = false;

		EnumeratePayloads(StructProperty, [&bHasValidData](const UScriptStruct* InScriptStruct, uint8* InMemory, const uint8*, UPackage*)
		{
			return (bHasValidData = InScriptStruct && InMemory); // Continue until invalid.
		});

		return bHasValidData;
	}

	virtual const UStruct* GetBaseStructure() const override
	{
		const UScriptStruct* CommonBaseStruct = nullptr;

		// Returns the most common base class for all edited data.
		EnumeratePayloads(StructProperty, [&CommonBaseStruct](const UScriptStruct* InScriptStruct, uint8* InMemory, const uint8*, UPackage*)
		{
			if (InScriptStruct)
			{
				while (InScriptStruct && CommonBaseStruct && !CommonBaseStruct->IsChildOf(InScriptStruct))
				{
					InScriptStruct = Cast<UScriptStruct>(InScriptStruct->GetSuperStruct());
				}

				CommonBaseStruct = InScriptStruct;
			}

			return true; // Continue.
		});

		return CommonBaseStruct;
	}

	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		// The returned instances need to be compatible with base structure.
		// This function returns empty instances in case they are not compatible, with the idea that we have as many instances as we have outer objects.
		EnumeratePayloads(StructProperty, [&OutInstances, ExpectedBaseStructure](const UScriptStruct* InScriptStruct, uint8* InMemory, const uint8*, UPackage* InPackage)
		{
			TSharedPtr<FStructOnScope>& Result = OutInstances.AddDefaulted_GetRef();

			if (InScriptStruct && InScriptStruct->IsChildOf(ExpectedBaseStructure))
			{
				Result = MakeShared<FStructOnScope>(InScriptStruct, InMemory);
				Result->SetPackage(InPackage);
			}

			return true; // Continue.
		});
	}

	virtual uint8* GetValueBaseAddress(uint8* ParentValueAddress, const UStruct* ExpectedBaseStructure) const override
	{
		if (FCommonItem* const CommonItem = reinterpret_cast<FCommonItem*>(ParentValueAddress))
		{
			const UScriptStruct* const ScriptStruct = CommonItem->GetPayload().GetScriptStruct();

			if (ExpectedBaseStructure && ScriptStruct && ScriptStruct->IsChildOf(ExpectedBaseStructure))
			{
				return CommonItem->GetMutablePayload().GetMutableMemory();
			}
		}

		return nullptr;
	}
};

/************************************************************************/
/* FCommonItemPayloadBuilder                                            */
/************************************************************************/

FCommonItemPayloadBuilder::~FCommonItemPayloadBuilder()
{
	UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Remove(UserDefinedStructReinstancedHandle);
}

FName FCommonItemPayloadBuilder::GetName() const
{
	static const FName Name("CommonItemInfoPayloadBuilder");
	return Name;
}

void FCommonItemPayloadBuilder::SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren)
{
	OnRegenerateChildren = InOnRegenerateChildren;
}

void FCommonItemPayloadBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	if (StructProperty.IsValid())
	{
		StructProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCommonItemPayloadBuilder::OnStructLayoutChanges));

		if (!UserDefinedStructReinstancedHandle.IsValid())
		{
			UserDefinedStructReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddSP(this, &FCommonItemPayloadBuilder::OnUserDefinedStructReinstanced);
		}
	}
}

void FCommonItemPayloadBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildBuilder)
{
	PayloadStructProvider = MakeShared<FCommonItemPayloadProvider>(StructProperty.ToSharedRef());

	for (TSharedPtr<IPropertyHandle> ChildHandle : StructProperty->AddChildStructure(PayloadStructProvider.ToSharedRef()))
	{
		// Add sub-properties and setup reset to default for them.
		OnChildRowAdded(ChildBuilder.AddProperty(ChildHandle.ToSharedRef()));

		// A temporary, incomplete solution for propagating changes to instances, since the default implementation doesn't do that at all for FStructurePropertyNode.
		// FInstancedStructDetails inherits the same behavior. There is also the 'problem' that propagation from the archetype only works with absolute payload values.
		ChildHandle->SetOnPropertyValuePreChange(FSimpleDelegate::CreateSP(this, &FCommonItemPayloadBuilder::OnChildPropertyPreChange, ChildHandle.ToSharedRef()));
		ChildHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FCommonItemPayloadBuilder::OnChildPropertyChanged, ChildHandle.ToSharedRef()));
	}
}

void FCommonItemPayloadBuilder::OnChildRowAdded(IDetailPropertyRow& ChildRow)
{
	auto IsResetVisible = [WeakPtr = AsWeak()](TSharedPtr<IPropertyHandle> PropertyHandle)
		{
			if (const TSharedPtr<FCommonItemPayloadBuilder> This = WeakPtr.Pin())
			{
				return !This->IsDefaultValue(PropertyHandle);
			}

			return false;
		};

	auto OnReset = [WeakPtr = AsWeak()](TSharedPtr<IPropertyHandle> PropertyHandle)
		{
			if (const TSharedPtr<FCommonItemPayloadBuilder> This = WeakPtr.Pin())
			{
				This->ResetToDefault(PropertyHandle);
			}
		};

	ChildRow.OverrideResetToDefault(FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateLambda(IsResetVisible), FResetToDefaultHandler::CreateLambda(OnReset), false));
}

void FCommonItemPayloadBuilder::OnStructLayoutChanges()
{
	if (PayloadStructProvider.IsValid())
	{
		// Reset the struct provider immediately, some update functions might get called with the old struct.
		PayloadStructProvider->Reset();
	}

	OnRegenerateChildren.ExecuteIfBound();
}

void FCommonItemPayloadBuilder::OnUserDefinedStructReinstanced(const UUserDefinedStruct& Struct)
{
	OnStructLayoutChanges();
}

void FCommonItemPayloadBuilder::OnChildPropertyPreChange(TSharedRef<IPropertyHandle> InChildProperty)
{
	if (StructProperty.IsValid() && PayloadProperty.IsValid())
	{
		ChildOriginalValues.Reset();

		TArray<UObject*> OuterObjects;
		PayloadProperty->GetOuterObjects(OuterObjects);

		// Just copy the original values before applying the changes.
		StructProperty->EnumerateRawData([&OuterObjects, this](void* RawData, const int32 DataIndex, const int32 /* NumData */)
		{
			if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData))
			{
				CommonItem->GetPayload().ExportTextItem(ChildOriginalValues.AddDefaulted_GetRef(), FVariadicStruct(), OuterObjects[DataIndex], PPF_ForDiff);
			}

			return true; // Continue.
		});
	}
}

void FCommonItemPayloadBuilder::OnChildPropertyChanged(TSharedRef<IPropertyHandle> InChildProperty)
{
	if (StructProperty.IsValid() && PayloadProperty.IsValid())
	{
		check(ChildOriginalValues.Num() == InChildProperty->GetNumPerObjectValues());

		TArray<UObject*> OuterObjects;
		PayloadProperty->GetOuterObjects(OuterObjects);

		TArray<FString> StructValues;
		StructProperty->EnumerateRawData([&OuterObjects, &StructValues, this](void* RawData, const int32 DataIndex, const int32 /* NumData */)
		{
			// Export the new value for further propagation and reset to the previous value since the old value is required for propagation.
			if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData))
			{
				FVariadicStruct& ItemPayload = CommonItem->GetMutablePayload();

				ItemPayload.ExportTextItem(StructValues.AddDefaulted_GetRef(), FVariadicStruct(), OuterObjects[DataIndex], PPF_ForDiff);
				const TCHAR* Buffer = *ChildOriginalValues[DataIndex];
				ItemPayload.ImportTextItem(Buffer);
			}

			return true; // Continue.
		});

		ChildOriginalValues.Reset();
		PayloadProperty->SetPerObjectValues(StructValues, EPropertyValueSetFlags::DefaultFlags);
	}
}

bool FCommonItemPayloadBuilder::IsDefaultValue(TSharedPtr<IPropertyHandle> InChildProperty)
{
	bool bIsDefault = true;

	if (const FProperty* const Property = InChildProperty->GetProperty())
	{
		EnumeratePayloads(StructProperty, [Property, &bIsDefault](const UScriptStruct*, uint8* InMemory, const uint8* InDefaults, UPackage*)
		{
			return (bIsDefault = Property->Identical(InMemory + Property->GetOffset_ForInternal(), InDefaults + Property->GetOffset_ForInternal(), PPF_None));
		});
	}

	return bIsDefault;
}

void FCommonItemPayloadBuilder::ResetToDefault(TSharedPtr<IPropertyHandle> InChildProperty)
{
	if (StructProperty.IsValid() && PayloadProperty.IsValid())
	{
		if (const FProperty* const Property = InChildProperty->GetProperty())
		{
			FScopedTransaction Transaction{ FText::Format(LOCTEXT("ResetToDefault", "Reset {0} to default value."), FText::FromName(Property->GetFName())) };
			PayloadProperty->NotifyPreChange();

			TArray<UObject*> OuterObjects;
			PayloadProperty->GetOuterObjects(OuterObjects);

			// We have to export/import at the payload level since FPropertyValueImpl::ImportText() doesn't handle FStructurePropertyNode correctly.
			TArray<FString> StructValues;
			StructProperty->EnumerateRawData([Property, &StructValues, &OuterObjects](void* RawData, const int32 DataIndex, const int32 /* NumData */)
			{
				// Don't reset the original value, instead create a dummy object, reset it and export it.
				if (FCommonItem* const CommonItem = static_cast<FCommonItem*>(RawData))
				{
					FVariadicStruct DummyPayload = CommonItem->GetPayload();
					const int32 PropertyOffset = Property->GetOffset_ForInternal();
					Property->CopyCompleteValue(DummyPayload.GetMutableMemory() + PropertyOffset, CommonItem->GetDefaultPayload().GetMemory() + PropertyOffset);
					DummyPayload.ExportTextItem(StructValues.AddDefaulted_GetRef(), FVariadicStruct(), OuterObjects[DataIndex], PPF_ForDiff);
				}

				return true; // Continue.
			});

			// This will update the archetype and correctly propagate changes across instances.
			PayloadProperty->SetPerObjectValues(StructValues, EPropertyValueSetFlags::DefaultFlags);
			PayloadProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
			PayloadProperty->NotifyFinishedChangingProperties();
		}
	}
}

/************************************************************************/
/* SCommonItemComboBox                                                  */
/************************************************************************/

void SCommonInventoryPrimaryAssetNameSelector::Construct(const FArguments& InArgs)
{
	AllowedArchetypes = InArgs._AllowedArchetypes;
	DisallowedArchetypes = InArgs._DisallowedArchetypes;
	OnSelectionChangedDelegate = InArgs._OnSelectionChanged;

	ChildSlot
	[
		SAssignNew(SearchableComboBox, SSearchableComboBox)
		.MaxListHeight(300.f)
		.HasDownArrow(true)
		.OptionsSource(&CachedItemList)
		.OnGenerateWidget(this, &SCommonInventoryPrimaryAssetNameSelector::OnGenerateComboWidget)
		.OnSelectionChanged(this, &SCommonInventoryPrimaryAssetNameSelector::OnSelectionChanged)
		.OnComboBoxOpening(this, &SCommonInventoryPrimaryAssetNameSelector::RefreshItemList)
		.ContentPadding(FMargin(2.0f, 2.0f))
		.Content()
		[
			SNew(STextBlock)
			.Text_Lambda([Value = InArgs._DisplayValue]() { return Value.IsSet() ? FText::FromString(Value.Get()) : FText::GetEmpty(); })
			.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		]
	];

	SearchableComboBox->SetEnabled(InArgs._IsEditable);
}

void SCommonInventoryPrimaryAssetNameSelector::RefreshItemList()
{
	TArray<FPrimaryAssetType, TInlineAllocator<12>> Archetypes{ AllowedArchetypes };

	if (Archetypes.IsEmpty())
	{
		UCommonInventoryRegistry::Get().GetArchetypes(Archetypes);

		if (!DisallowedArchetypes.IsEmpty())
		{
			Archetypes.RemoveAll([this](FPrimaryAssetType InArchetype) { return DisallowedArchetypes.Contains(InArchetype); });
		}
	}

	CachedItemList.Reset();
	CachedItemList.Add(MakeShared<FString>(TEXT("None")));

	for (const FPrimaryAssetType Archetype : Archetypes)
	{
		for (const FCommonInventoryRegistryRecord& Record : UCommonInventoryRegistry::Get().GetRegistryRecords(Archetype))
		{
			CachedItemList.Add(MakeShared<FString>(Record.GetPrimaryAssetName().ToString()));
		}
	}

	SearchableComboBox->RefreshOptions();
}

TSharedRef<SWidget> SCommonInventoryPrimaryAssetNameSelector::OnGenerateComboWidget(TSharedPtr<FString> InComboString)
{
	if (InComboString.IsValid())
	{
		const FName PrimaryAssetName = InComboString.Get()->operator*();
		const FSlateBrush* Brush = nullptr;
		FColor IconColor = FColor::White;

		if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().FindRegistryRecordFromName(PrimaryAssetName))
		{
			if (const UObject* const Asset = Record->AssetPath.ResolveObject())
			{
				Brush = FSlateIconFinder::FindIconBrushForClass(Asset->GetClass());
				const TWeakPtr<IAssetTypeActions> TypeActions = FAssetToolsModule::GetModule().Get().GetAssetTypeActionsForClass(Asset->GetClass());

				if (TypeActions.IsValid())
				{
					IconColor = TypeActions.Pin()->GetTypeColor();
				}
			}
		}

		const FText TextValue = FText::FromString(*InComboString);

		return SNew(SBox)
		.MaxDesiredWidth(0.f) // This will clamp the width by SSearchableComboBox.
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot() // Icon
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(16.f)
				.HeightOverride(16.f)
				[
					SNew(SImage)
					.Image(Brush)
					.ColorAndOpacity(IconColor)
				]
			]
			+ SHorizontalBox::Slot() // Name
			.AutoWidth()
			.Padding(7.f, 2.f, 2.f, 2.f)
			[
				SNew(STextBlock)
				.Text(TextValue)
				.ToolTipText(TextValue)
				.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];
	}

	return SNullWidget::NullWidget;
}

COMMON_INVENTORY_IMPLEMENT_GET_PRIVATE_MEMBER(SSearchableComboBox, SelectedItem, TSharedPtr<FString>);
void SCommonInventoryPrimaryAssetNameSelector::OnSelectionChanged(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	if (InSelectedItem.IsValid())
	{
		// Don't trigger a full rebuild, invalidating the popup menu.
		if (SelectInfo != ESelectInfo::OnNavigation)
		{
			const FName PrimaryAssetName = InSelectedItem.Get()->operator*();
			FPrimaryAssetId SelectedPrimaryAssetId;

			if (const FCommonInventoryRegistryRecord* const Record = UCommonInventoryRegistry::Get().FindRegistryRecordFromName(PrimaryAssetName))
			{
				SelectedPrimaryAssetId = Record->GetPrimaryAssetId();
			}

			OnSelectionChangedDelegate.ExecuteIfBound(SelectedPrimaryAssetId);
		}
		else
		{
			// Invalidate the SelectedItem so we can receive the 'Enter' key event later. @see SSearchableComboBox::OnSelectionChanged_Internal
			COMMON_INVENTORY_GET_PRIVATE_MEMBER(SSearchableComboBox, *SearchableComboBox.Get(), SelectedItem) = TSharedPtr<FString>();
		}
	}
}

#undef LOCTEXT_NAMESPACE
