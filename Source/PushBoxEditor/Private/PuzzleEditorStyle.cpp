// Copyright PushBox Game. All Rights Reserved.

#include "PuzzleEditorStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(Style->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

TSharedPtr<FSlateStyleSet> FPuzzleEditorStyle::StyleInstance = nullptr;

void FPuzzleEditorStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FPuzzleEditorStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

void FPuzzleEditorStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FPuzzleEditorStyle::Get()
{
	return *StyleInstance;
}

FName FPuzzleEditorStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("PushBoxEditorStyle"));
	return StyleSetName;
}

TSharedRef<FSlateStyleSet> FPuzzleEditorStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShareable(new FSlateStyleSet(GetStyleSetName()));
	Style->SetContentRoot(FPaths::ProjectDir() / TEXT("Resources"));

	// Use a default icon ¡ª custom icons can be added to Resources/ folder later
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);

	// Register default brush as fallback (won't crash if icon file is missing)
	Style->Set("PushBoxEditor.OpenPuzzleEditor",
		new FSlateImageBrush(FPaths::EngineContentDir() / TEXT("Editor/Slate/Icons/icon_tab_Toolbox_16x.png"), Icon16x16));

	// Grid cell colors for the editor preview
	Style->Set("PushBoxEditor.Cell.Floor", new FSlateColorBrush(FLinearColor(0.3f, 0.3f, 0.3f)));
	Style->Set("PushBoxEditor.Cell.Wall", new FSlateColorBrush(FLinearColor(0.1f, 0.1f, 0.1f)));
	Style->Set("PushBoxEditor.Cell.Box", new FSlateColorBrush(FLinearColor(0.8f, 0.5f, 0.1f)));
	Style->Set("PushBoxEditor.Cell.Target", new FSlateColorBrush(FLinearColor(0.2f, 0.8f, 0.2f)));
	Style->Set("PushBoxEditor.Cell.Player", new FSlateColorBrush(FLinearColor(0.2f, 0.4f, 0.9f)));
	Style->Set("PushBoxEditor.Cell.Ice", new FSlateColorBrush(FLinearColor(0.6f, 0.9f, 1.0f)));
	Style->Set("PushBoxEditor.Cell.OneWayDoor", new FSlateColorBrush(FLinearColor(0.9f, 0.7f, 0.2f)));
	Style->Set("PushBoxEditor.Cell.TargetMarker", new FSlateColorBrush(FLinearColor(0.4f, 0.9f, 0.4f)));

	return Style;
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
