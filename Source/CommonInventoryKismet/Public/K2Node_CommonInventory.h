// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "K2Node_CallFunction.h"
#include "K2Node_CommonInventory.generated.h"

class UEdGraphPin;
class FBlueprintActionDatabaseRegistrar;

/**
 * Node customization for CustomThunk functions from UCommonInventoryFunctionLibrary.
 */
UCLASS()
class UK2Node_CommonInventory : public UK2Node_CallFunction
{
	GENERATED_BODY()

	//~ Begin UEdGraphNode Interface
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	//~ End UEdGraphNode Interface

	//~ Begin K2Node Interface
	virtual bool IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const override;
	//~ End K2Node Interface
};