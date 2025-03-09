// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "K2Node_CommonInventory.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "CommonInventory/Private/CommonInventoryFunctionLibrary.h"
#include "EdGraphSchema_K2.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(K2Node_CommonInventory)

#define LOCTEXT_NAMESPACE "CommonInventory"

void UK2Node_CommonInventory::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	Super::GetMenuActions(ActionRegistrar);

	if (ActionRegistrar.IsOpenForRegistration(GetClass()))
	{
		auto RegisterFunction = [this, &ActionRegistrar](FName InFunctionName)
			{
				auto CustomizeLambda = [](UEdGraphNode* NewNode, bool bIsTemplateNode, const FName FunctionName)
					{
						UK2Node_CommonInventory* Node = CastChecked<UK2Node_CommonInventory>(NewNode);
						UFunction* Function = UCommonInventoryFunctionLibrary::StaticClass()->FindFunctionByName(FunctionName);
						check(Function);
						Node->SetFromFunction(Function);
					};

				UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
				check(NodeSpawner != nullptr);
				NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateStatic(CustomizeLambda, InFunctionName);
				ActionRegistrar.AddBlueprintAction(GetClass(), NodeSpawner);
			};

		RegisterFunction(GET_FUNCTION_NAME_CHECKED(UCommonInventoryFunctionLibrary, GetRegistryDefaultPayload));
		RegisterFunction(GET_FUNCTION_NAME_CHECKED(UCommonInventoryFunctionLibrary, GetRegistryCustomData));
		RegisterFunction(GET_FUNCTION_NAME_CHECKED(UCommonInventoryFunctionLibrary, GetPayloadValue));
		RegisterFunction(GET_FUNCTION_NAME_CHECKED(UCommonInventoryFunctionLibrary, SetPayloadValue));
	}
}

bool UK2Node_CommonInventory::IsConnectionDisallowed(const UEdGraphPin* MyPin, const UEdGraphPin* OtherPin, FString& OutReason) const
{
	const UEdGraphPin* ValuePin = FindPinChecked(FName(TEXT("Value")));

	if (MyPin == ValuePin && MyPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard)
	{
		if (OtherPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct)
		{
			OutReason = TEXT("Value must be a struct.");
			return true;
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
