// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

#include "CommonInventoryView.generated.h"

class UCommonInventoryComponent;

/**
 * 
 */
USTRUCT(/* BlueprintType, */ meta = (Hidden))
struct COMMONINVENTORY_API FCommonInventoryTraversingParams
{
	GENERATED_BODY()

	/**  */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	bool bIncludeEmptyItems = false;

	/**  */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	bool bIncludeRedirects = false;

	/**  */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	TSet<FPrimaryAssetType> ArchetypesFilter;

	/**  */
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	TObjectPtr<const UScriptStruct> PayloadFilter;
};

/**
 * 
 */
USTRUCT(/* BlueprintType, */ meta = (Hidden))
struct COMMONINVENTORY_API FCommonItemView
{
	GENERATED_BODY()
};

/**
 * 
 */
USTRUCT(/* BlueprintType, */ meta = (Hidden))
struct COMMONINVENTORY_API FCommonInventoryView
{
	GENERATED_BODY()

	// Add filtering, sorting, enumeration, etc.
	// Create custom BP node for iterating.
	// https://www.gamedev.net/tutorials/programming/engines-and-middleware/improving-ue4-blueprint-usability-with-custom-nodes-r5694/

	struct FStateData
	{
		TWeakObjectPtr<UCommonInventoryComponent> OriginalComponent;
	};

	TSharedPtr<FStateData> SharedState;
};
