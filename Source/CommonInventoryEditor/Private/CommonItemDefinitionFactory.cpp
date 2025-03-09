// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonItemDefinitionFactory.h"
#include "CommonItemDefinition.h"

#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Internationalization/Text.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Modules/ModuleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonItemDefinitionFactory)

constexpr EClassFlags DisallowedClassFlags = CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists | CLASS_HideDropDown;

namespace
{
	//Based on FAssetClassParentFilter from EditorFactories.cpp
	class FCommonItemDefinitionAssetFilter : public IClassViewerFilter
	{
		virtual bool IsClassAllowed(const FClassViewerInitializationOptions&, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs>) override
		{
			return InClass->CanCreateAssetOfClass()
				&& !InClass->HasAnyClassFlags(DisallowedClassFlags)
				&& InClass->IsChildOf(UCommonItemDefinition::StaticClass());
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions&, const TSharedRef<const IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<FClassViewerFilterFuncs> ) override
		{
			return !InUnloadedClassData->HasAnyClassFlags(DisallowedClassFlags)
				&& InUnloadedClassData->IsChildOf(UCommonItemDefinition::StaticClass());
		}
	};
}

UCommonItemDefinitionFactory::UCommonItemDefinitionFactory()
{
	SupportedClass = UCommonItemDefinition::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

bool UCommonItemDefinitionFactory::ConfigureProperties()
{
	PickedClass = nullptr;

	// Load the ClassViewer module to display a class picker
	FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer");

	// Fill in options
	FClassViewerInitializationOptions Options;
	Options.ClassFilters.Emplace(MakeShared<FCommonItemDefinitionAssetFilter>());
	Options.Mode = EClassViewerMode::ClassPicker;
	Options.DisplayMode = EClassViewerDisplayMode::ListView;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;
	Options.InitiallySelectedClass = SupportedClass;

	UClass* ChosenClass = nullptr;
	const FText TitleText = NSLOCTEXT("CommonInventoryEditor", "ItemDefinitionPickerDialog", "Pick Class For Item Definition Instance");

	if (SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, SupportedClass))
	{
		PickedClass = ChosenClass;
		return true;
	}

	return false;
}

UObject* UCommonItemDefinitionFactory::FactoryCreateNew(UClass* Class, UObject* InParent, const FName Name, const EObjectFlags Flags, UObject*, FFeedbackContext*)
{
	const EObjectFlags FlagsToUse = PickedClass.Get() ? Flags | RF_Transactional : Flags;
	const UClass* const ClassToUse = PickedClass.Get() ? PickedClass.Get() : Class;
	return NewObject<UCommonItemDefinition>(InParent, ClassToUse, Name, FlagsToUse);
}

FString UCommonItemDefinitionFactory::GetDefaultNewAssetName() const
{
	return FString("ID_");
}
