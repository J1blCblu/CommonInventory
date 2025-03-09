// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Engine/DeveloperSettings.h"
#include "Math/UnrealMathUtility.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "CommonInventorySettings.generated.h"

/**
 * Used to more easily handle merge conflicts in the config.
 * 
 *	[/Script/MyScriptPackageName.MyClass]
 *	MyMap=((Key, Value),(Key1, Value1), ... (KeyN, ValueN))
 * 
 *	+MyArray=(Key,Value)
 *	+MyArray=(Key1,Value2)
 *	...
 *  +MyArray=(KeyN,ValueN)
 */
USTRUCT(Atomic, meta = (Hidden))
struct FCommonInventoryRedirector
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "CommonInventoryRedirector")
	FName OldValue;

	UPROPERTY(EditDefaultsOnly, Category = "CommonInventoryRedirector")
	FName NewValue;

public:

	bool IsValid() const
	{
		return !OldValue.IsNone() && !NewValue.IsNone() && OldValue != NewValue;
	}

	bool HasValue(const FName Value) const
	{
		return OldValue == Value || NewValue == Value;
	}

	void SwapValues()
	{
		::Swap(OldValue, NewValue);
	}

	friend bool operator==(const FCommonInventoryRedirector&, const FCommonInventoryRedirector&) = default;
};

/**
 * Common Inventory per project settings.
 */
UCLASS(MinimalAPI, Config = Game, DefaultConfig, meta = (DisplayName = "Common Inventory"))
class UCommonInventorySettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UCommonInventorySettings() = default;

	static const UCommonInventorySettings* Get()
	{
		return GetDefault<ThisClass>();
	}

	static UCommonInventorySettings* GetMutable()
	{
		return GetMutableDefault<ThisClass>();
	}

public: //Inventory Registry

	/** Responsible for gathering archetype/shared data for the registry. */
	UPROPERTY(Config, EditDefaultsOnly, NoClear, Category = "InventoryRegistry", meta = (MetaClass = "/Script/CommonInventory.CommonInventoryRegistryDataSource", DisplayName = "Data Source Class", ConfigRestartRequired = true))
	FSoftClassPath DataSourceClassName = FSoftClassPath(TEXT("/Script/CommonInventory.AssetManagerDataSource"));

	/** Whether the registry should reflect its checksum through the custom version in FNetworkVersion. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InventoryRegistry", meta = (ConfigRestartRequired = true))
	bool bRegisterNetworkCustomVersion = true;

#if WITH_EDITORONLY_DATA

	/** Whether to validate complying of names with the naming convention pattern. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InventoryRegistry|Data Validation", meta = (InlineEditConditionToggle))
	bool bValidateNamingConvention = true;

	/** A regular expression pattern to validate names. The default is ID_PPPascalCase. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InventoryRegistry|Data Validation", meta = (EditCondition = bValidateNamingConvention, DisplayName = "Validate Naming Convention"))
	FString NamingConventionPattern = TEXT(R"(^ID_([A-Z]*(?:[A-Z][a-z0-9]+)+)$)");

#endif

	/** List of FPrimaryAssetName redirectors. Necessary to prevent corruption of persistent data such as client saves and engine packages after refactoring. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InventoryRegistry|Redirection", meta = (NoElementDuplicate, EditFixedOrder, TitleProperty = "{OldValue} to {NewValue}", ConfigRestartRequired = true))
	TArray<FCommonInventoryRedirector> PrimaryAssetNameRedirects;

	/** List of FPrimaryAssetType redirectors. Necessary to prevent corruption of persistent data such as client saves and engine packages after refactoring. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "InventoryRegistry|Redirection", meta = (NoElementDuplicate, EditFixedOrder, TitleProperty = "{OldValue} to {NewValue}", ConfigRestartRequired = true))
	TArray<FCommonInventoryRedirector> PrimaryAssetTypeRedirects;

public: // Networking

	/** The maximum number of commands in the queue at the same time. Overflowing this value will result in the player being disconnected. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Networking")
	int32 CommandQueueLength = 10;

	/** CommandQueue flush rate for autonomous proxies. Commands get accumulated in between updates. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Networking", meta = (Units = "s"))
	float CommandQueueFlushRate = .2f;

	/** Limits the total available capacity for all inventories. Exceeding this value will result in the player being disconnected. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Networking")
	int32 InventoryCapacityLimit = 256;

public:

	virtual FName GetCategoryName() const override { return NAME_Game; }
	virtual FName GetSectionName() const override { return FName("Common Inventory"); }

#if WITH_EDITOR
	COMMONINVENTORY_API virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	COMMONINVENTORY_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
