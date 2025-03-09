// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CommonInventoryState.h"
#include "CommonInventoryView.h"
#include "Templates/UniquePtr.h"

#include "CommonInventoryComponent.generated.h"

class APlayerController;
class UCommonInventoryState;

struct FCommonInventoryCommandController;

/** All available replication modes for UCommonInventoryComponent. */
UENUM()
enum class ECommonInventoryReplicationMode : uint8
{
	Never		UMETA(ToolTip = "Not replicated."),
	Public		UMETA(ToolTip = "Replicated to all relevant connections. Not recommended for large inventories or under bandwidth limited conditions."),
	Protected	UMETA(ToolTip = "Replicated only to manually registered connections. Recommended for non-player entities."),
	Private		UMETA(ToolTip = "Replicated only to the owning connection. Recommended for player owned entities."),
	Auto		UMETA(ToolTip = "Replicated only to automatically registered connections by the prefetcher."),
};

/**
 * CommonInventoryComponent is a backend storage for items with a consistent transactional network model based on commands.
 */
UCLASS(Hidden, HideCategories = "ComponentReplication")
class COMMONINVENTORY_API UCommonInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

	UCommonInventoryComponent();

	// Avoid issues with TUniquePtr.
	~UCommonInventoryComponent();

public:

	/** Creates a non-intrusive copy of the inventory that supports filtering, sorting, etc. */
	//UFUNCTION(BlueprintCallable, Category = "CommonInventory|View")
	FCommonInventoryView MakeInventoryView(const FCommonInventoryTraversingParams& TraversingParams) const;

public: // Server Only

	/** [Server] */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="CommonInventory|Networking")
	void RegisterInventoryListener(const APlayerController* PlayerController);

	/** [Server] */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="CommonInventory|Networking")
	void UnregisterInventoryListener(const APlayerController* PlayerController);

public: // Overrides

//	virtual void OnRegister() override;
//	virtual void PostLoad() override;
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
//	virtual void PreNetReceive() override;
//	virtual void PostNetReceive() override;
//	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
//	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual ELifetimeCondition GetReplicationCondition() const override;
	virtual bool GetComponentClassCanReplicate() const override;

protected:

	FCommonInventoryCommandController* GetCommandController() const;

protected:

	//UPROPERTY(EditAnywhere, Category = "Common Inventory")
	//TSoftObjectPtr<class UCommonInventoryDefaultPreset> DefaultPreset;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Initialization")
	int32 DefaultCapacity = 16;

	/**  */
	UPROPERTY(EditAnywhere, Category = "Initialization")
	bool bIsAutomatic = false;

	/** All supported replication modes of the inventory. */
	UPROPERTY(EditAnywhere, Category = "Networking")
	ECommonInventoryReplicationMode ReplicationMode;

private:

	/**  */
	UPROPERTY(Replicated)
	FCommonInventoryState InventoryState;

	/**  */
	mutable TUniquePtr<FCommonInventoryCommandController> CommandController;
};
