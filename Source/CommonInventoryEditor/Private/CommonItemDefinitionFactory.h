// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "Templates/SubclassOf.h"
#include "CommonItemDefinitionFactory.generated.h"

class UCommonItemDefinition;

/**
 * Factory responsible to create CommonItemDefinitions.
 */
UCLASS(MinimalAPI)
class UCommonItemDefinitionFactory : public UFactory
{
	GENERATED_BODY()

	UCommonItemDefinitionFactory();

	UPROPERTY()
	TSubclassOf<UCommonItemDefinition> PickedClass;

protected: //Overrides

	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject*, FFeedbackContext*) override;
	virtual FString GetDefaultNewAssetName() const override;
};
