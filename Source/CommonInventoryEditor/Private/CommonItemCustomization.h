// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "IDetailCustomNodeBuilder.h"
#include "IPropertyTypeCustomization.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "UObject/PrimaryAssetId.h"
#include "Widgets/SCompoundWidget.h"

// Defined in cpp.
class FCommonItemPayloadProvider;

class FReply;
class IDetailPropertyRow;
class SSearchableComboBox;
class SToolTip;
class SWidget;
class UUserDefinedStruct;

/**
 * Details customization for FCommonItem.
 */
class FCommonItemCustomization : public IPropertyTypeCustomization
{
public:

	//@TODO: Check for InstancedStructDetails in UE5.5.

	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FCommonItemCustomization>();
	}

	~FCommonItemCustomization();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& InStructBuilder, IPropertyTypeCustomizationUtils& InStructCustomizationUtils) override;

private:

	void ParseMetadata(TArray<FPrimaryAssetType>& OutAllowedArchetypes, TArray<FPrimaryAssetType>& OutDisallowedArchetypes);

	void OnResetPayload();
	void OnObjectsReinstanced(const FCoreUObjectDelegates::FReplacementObjectMap& ObjectMap);

	void OnPrimaryAssetIdSelected(FPrimaryAssetId PrimaryAssetId);
	FString GetDisplayValueAsString() const;

	// Button events
	void OnClear();
	void OnBrowseTo();
	FReply OnEditAsset();
	FReply OnFindReferencers();

private:

	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyHandle> PrimaryAssetIdChildProperty;
	TSharedPtr<IPropertyHandle> PayloadChildProperty;

	TSharedPtr<IPropertyUtilities> PropertyUtilities;
	
	FDelegateHandle OnForceRefreshDetailsHandle;
	FDelegateHandle OnObjectsReinstancedHandle;
	FDelegateHandle OnRegistryRefreshedHandle;
};

/**
 * Payload node builder for FCommonItem.
 */
class FCommonItemPayloadBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FCommonItemPayloadBuilder>
{
public:

	FCommonItemPayloadBuilder(TSharedPtr<IPropertyHandle> InStructProperty, TSharedPtr<IPropertyHandle> InPayloadProperty)
		: StructProperty(InStructProperty)
		, PayloadProperty(InPayloadProperty)
	{
		// Do something.
	}
	
	virtual ~FCommonItemPayloadBuilder() override;

	//~ Begin IDetailCustomNodeBuilder interface
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override;
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override;
	//~ End IDetailCustomNodeBuilder interface

private:
	
	void OnChildRowAdded(IDetailPropertyRow& ChildRow);

	void OnStructLayoutChanges();
	void OnUserDefinedStructReinstanced(const UUserDefinedStruct& Struct);

	void OnChildPropertyPreChange(TSharedRef<IPropertyHandle> InChildProperty);
	void OnChildPropertyChanged(TSharedRef<IPropertyHandle> InChildProperty);
	bool IsDefaultValue(TSharedPtr<IPropertyHandle> InPayloadProperty);
	void ResetToDefault(TSharedPtr<IPropertyHandle> InPayloadProperty);

private:

	TSharedPtr<IPropertyHandle> StructProperty;
	TSharedPtr<IPropertyHandle> PayloadProperty;

	TSharedPtr<FCommonItemPayloadProvider> PayloadStructProvider;
	TArray<FString, TInlineAllocator<8>> ChildOriginalValues;
	FDelegateHandle UserDefinedStructReinstancedHandle;
	FSimpleDelegate OnRegenerateChildren;
};

/**
 * 
 */
class SCommonInventoryPrimaryAssetNameSelector : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SCommonInventoryPrimaryAssetNameSelector) {}

		SLATE_ARGUMENT(TArray<FPrimaryAssetType>, AllowedArchetypes)
		SLATE_ARGUMENT(TArray<FPrimaryAssetType>, DisallowedArchetypes)

		SLATE_ATTRIBUTE(bool, IsEditable)
		SLATE_ATTRIBUTE(FString, DisplayValue)

		SLATE_EVENT(TDelegate<void(FPrimaryAssetId)>, OnSelectionChanged)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:

	void RefreshItemList();
	TSharedRef<SWidget> OnGenerateComboWidget(TSharedPtr<FString> InComboString);
	void OnSelectionChanged(TSharedPtr<FString> InSelectedItem, ESelectInfo::Type SelectInfo);

private:

	TSharedPtr<SSearchableComboBox> SearchableComboBox;
	TArray<TSharedPtr<FString>> CachedItemList;

	TArray<FPrimaryAssetType> AllowedArchetypes;
	TArray<FPrimaryAssetType> DisallowedArchetypes;
	TDelegate<void(FPrimaryAssetId)> OnSelectionChangedDelegate;
};
