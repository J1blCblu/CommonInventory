// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "CommonInventoryTypes.h"
#include "Engine/DataAsset.h"
#include "InstancedStruct.h"
#include "CommonItemDefinition.generated.h"

/**
 * A container for static data shared between multiple item instances of the same type.
 * Since the core functionality uses UCommonInventoryRegistry to more efficiently access the actual data,
 * UAssetManagerDataSource should be specified in the config for proper integration with the registry.
 * 
 * @Note: UCommonItemDefinition can be sub-classed to create a new archetype group and add custom properties.
 * 
 * @see UCommonInventoryRegistry, UCommonItemDefinition, FCommonItem.
 */
UCLASS(MinimalAPI, Blueprintable, Const)
class UCommonItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

	//@TODO: FInstancedStruct has a more granular filtering setup in UE5.5. Need to exclude types with object refs.

public:

	/** Default payload for FCommonItem accessible from UCommonInventoryRegistry. */
	UPROPERTY(EditDefaultsOnly, Category = "Item Payload")
	FInstancedStruct DefaultItemPayload;

	/** Shared data accessible from UCommonInventoryRegistry. */
	UPROPERTY(EditDefaultsOnly, Category = "Shared Data", meta = (ShowOnlyInnerProperties))
	FCommonItemSharedData SharedData;

	/** Optional user defined data accessible from UCommonInventoryRegistry. */
	UPROPERTY(EditDefaultsOnly, Category = "Registry Data")
	FInstancedStruct RegistryCustomData;

public: // Overrides

	COMMONINVENTORY_API virtual void PostInitProperties() override;
	COMMONINVENTORY_API virtual bool IsPostLoadThreadSafe() const override { return true; }
	COMMONINVENTORY_API virtual bool NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const override final { return true; }

#if WITH_EDITOR
	COMMONINVENTORY_API virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	COMMONINVENTORY_API virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
