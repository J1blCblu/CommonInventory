// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CommonInventoryRegistryDataSource.generated.h"

class ICommonInventoryRegistryBridge;
class ITargetPlatform;

/**
 * Responsible for gathering archetype/shared data for the registry.
 * Possible data sources: UAssetManager, UDataTable, service backend.
 * Data source allows to update archetype items without patching the game.
 */
UCLASS(Abstract)
class COMMONINVENTORY_API UCommonInventoryRegistryDataSource : public UObject
{
	GENERATED_BODY()

public:

	UCommonInventoryRegistryDataSource() = default;

	struct FStaticTraits
	{
		/** Whether the registry state is persistent during the game. */
		uint8 bIsPersistent : 1 = true;

		/** Whether the registry can be cooked along with the game. */
		uint8 bSupportsCooking : 1 = true;

		/** Whether the registry can be cooked along with the editor. */
		uint8 bSupportsDevelopmentCooking : 1 = true;
	};

	/** Returns the traits that are part of the registry contract. */
	FStaticTraits GetTraits() const { return Traits; }

public: // Lifetime

	/** Opens the Data Source initialization window. Called right after reading the cooked data. */
	virtual void Initialize(ICommonInventoryRegistryBridge* InRegistryBridge);

	/** Closes the Data Source initialization window for persistent registries. */
	virtual void PostInitialize();

	/** Finishes the lifetime of the data source. */
	virtual void Deinitialize();

	/** Whether the data source is initialized. */
	bool IsInitialized() const { return bIsInitialized; }

public: // Refreshing

	/** Force refresh the registry state. */
	virtual void ForceRefresh(bool bSynchronous = false);

	/** Flushes any pending refreshes. */
	virtual void FlushPendingRefresh();

	/** Cancels any pending refreshes. */
	virtual void CancelPendingRefresh();

	/** Whether refreshing was requested. */
	virtual bool IsPendingRefresh() const;

	/** Whether refreshing is in progress. */
	virtual bool IsRefreshing() const;

public: // Cooking

#if WITH_EDITOR

	/** Called to transform data into the cooking state. */
	virtual void OnCookStarted();

	/** Called to transform data back from the cooked state. */
	virtual void OnCookFinished();

	/** Called right before saving the registry state to verify assumptions. */
	virtual bool VerifyAssumptionsForCook(const ITargetPlatform* InTargetPlatform) const;

#endif // WITH_EDITOR

public: // Modular Features

#if WITH_EDITOR

	/** Whether we can start a play-in-editor session. @see IPIEAuthorizer::RequestPIEPermission. */
	virtual bool RequestPIEPermission(FString& OutReason, bool bIsSimulating) const;

#endif // WITH_EDITOR

protected:

	/** Registry bridge for communicating with the registry. */
	ICommonInventoryRegistryBridge* RegistryBridge = nullptr;

	/** Traits cached within the registry from CDO. */
	FStaticTraits Traits;

	/** Whether the registry is initialized. */
	bool bIsInitialized = false;
};

// Data source traits alias.
using FCommonInventoryRegistryDataSourceTraits = UCommonInventoryRegistryDataSource::FStaticTraits;
