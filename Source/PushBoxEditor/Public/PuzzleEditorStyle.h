// Copyright PushBox Game. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/**
 * Custom style set for PushBox editor icons and styling.
 */
class FPuzzleEditorStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static void ReloadTextures();

	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:
	static TSharedRef<class FSlateStyleSet> Create();
	static TSharedPtr<class FSlateStyleSet> StyleInstance;
};
