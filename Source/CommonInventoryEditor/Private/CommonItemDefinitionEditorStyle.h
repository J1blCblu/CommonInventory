// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FCommonItemDefinitionEditorStyle final : public FSlateStyleSet
{
public:

	//Color for an asset.
	static inline constexpr FColor TypeColor = FColor(166, 186, 13);

	virtual ~FCommonItemDefinitionEditorStyle();
	static FCommonItemDefinitionEditorStyle& Get();
	static void Shutdown();

private:

	FCommonItemDefinitionEditorStyle();
};
