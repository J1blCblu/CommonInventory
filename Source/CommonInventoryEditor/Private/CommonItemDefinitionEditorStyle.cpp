// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonItemDefinitionEditorStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"

static TUniquePtr<FCommonItemDefinitionEditorStyle> Instance = nullptr;

FCommonItemDefinitionEditorStyle::FCommonItemDefinitionEditorStyle() : FSlateStyleSet("CommonItemDefinitionEditorStyle")
{
	SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate/Icons/AssetIcons"));

	Set("ClassIcon.CommonItemDefinition", new IMAGE_BRUSH("Note_16x", CoreStyleConstants::Icon16x16));
	Set("ClassThumbnail.CommonItemDefinition", new IMAGE_BRUSH("Note_64x", CoreStyleConstants::Icon64x64));
	//Set("ClassIcon.CommonInventoryComponent", new IMAGE_BRUSH_SVG("Note_16x", CoreStyleConstants::Icon16x16));
	//Set("ClassThumbnail.CommonInventoryComponent", new IMAGE_BRUSH_SVG("Note_64x", CoreStyleConstants::Icon64x64));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FCommonItemDefinitionEditorStyle::~FCommonItemDefinitionEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FCommonItemDefinitionEditorStyle& FCommonItemDefinitionEditorStyle::Get()
{
	if (!Instance.IsValid())
	{
		Instance = TUniquePtr<FCommonItemDefinitionEditorStyle>(new FCommonItemDefinitionEditorStyle());
	}

	return *Instance.Get();
}

void FCommonItemDefinitionEditorStyle::Shutdown()
{
	Instance.Reset();
}
