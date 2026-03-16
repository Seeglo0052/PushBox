// Copyright PushBox Game. All Rights Reserved.

#include "Utilities/PushBoxFunctionLibrary.h"
#include "Core/PushBoxSubsystem.h"
#include "Grid/PushBoxGridManager.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceConstant.h"
#include "UObject/SoftObjectPath.h"
#include "Kismet/KismetSystemLibrary.h"

UPushBoxGridManager* UPushBoxFunctionLibrary::GetGridManager(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return nullptr;
	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return nullptr;

	UPushBoxSubsystem* Sub = World->GetSubsystem<UPushBoxSubsystem>();
	return Sub ? Sub->GetGridManager() : nullptr;
}

FString UPushBoxFunctionLibrary::DirectionToString(EPushBoxDirection Direction)
{
	switch (Direction)
	{
	case EPushBoxDirection::North: return TEXT("North");
	case EPushBoxDirection::East:  return TEXT("East");
	case EPushBoxDirection::South: return TEXT("South");
	case EPushBoxDirection::West:  return TEXT("West");
	default: return TEXT("None");
	}
}

FString UPushBoxFunctionLibrary::TileTypeToString(EPushBoxTileType TileType)
{
	switch (TileType)
	{
	case EPushBoxTileType::Empty:        return TEXT("Floor");
	case EPushBoxTileType::Wall:         return TEXT("Wall");
	case EPushBoxTileType::Ice:          return TEXT("Ice");
	case EPushBoxTileType::OneWayDoor:   return TEXT("OneWayDoor");
	case EPushBoxTileType::PlayerGoal:   return TEXT("PlayerGoal");
	case EPushBoxTileType::TargetMarker: return TEXT("TargetMarker");
	default: return TEXT("Unknown");
	}
}

TArray<FPushBoxMoveRecord> UPushBoxFunctionLibrary::ReverseMoveSequence(const TArray<FPushBoxMoveRecord>& InMoves)
{
	TArray<FPushBoxMoveRecord> Result;
	Result.Reserve(InMoves.Num());

	// Walk backward through the reverse moves and flip each one
	for (int32 i = InMoves.Num() - 1; i >= 0; --i)
	{
		const FPushBoxMoveRecord& Src = InMoves[i];
		FPushBoxMoveRecord Flipped;
		Flipped.Direction = UPushBoxGridManager::GetOppositeDirection(Src.Direction);
		Flipped.PlayerFrom = Src.PlayerTo;
		Flipped.PlayerTo = Src.PlayerFrom;
		Flipped.BoxFrom = Src.BoxTo;
		Flipped.BoxTo = Src.BoxFrom;
		Flipped.bIsReverse = false;
		Result.Add(Flipped);
	}

	return Result;
}

// ==================================================================
// Runtime Gameplay
// ==================================================================

void UPushBoxFunctionLibrary::RestartCurrentPuzzle(const UObject* WorldContextObject)
{
	UPushBoxGridManager* GM = GetGridManager(WorldContextObject);
	if (GM)
	{
		GM->ResetToStart();
		UE_LOG(LogPushBox, Log, TEXT("RestartCurrentPuzzle: Puzzle reset to start state."));
	}
	else
	{
		UE_LOG(LogPushBox, Warning, TEXT("RestartCurrentPuzzle: No active grid manager found."));
	}
}

void UPushBoxFunctionLibrary::QuitGame(const UObject* WorldContextObject)
{
	if (!WorldContextObject) return;

	UWorld* World = WorldContextObject->GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		UKismetSystemLibrary::QuitGame(WorldContextObject, PC, EQuitPreference::Quit, false);
	}
}

// ==================================================================
// Theme Color
// ==================================================================

static const TCHAR* GAtlasTexturePaths[] =
{
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas.T_Atlas"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_Blue.T_Atlas_Blue"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_Green.T_Atlas_Green"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_Grey.T_Atlas_Grey"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_Orange.T_Atlas_Orange"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_Red.T_Atlas_Red"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_White.T_Atlas_White"),
	TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Textures/BaseColor/T_Atlas_Yellow.T_Atlas_Yellow"),
};
static const int32 GAtlasTextureCount = UE_ARRAY_COUNT(GAtlasTexturePaths);

TArray<FSoftObjectPath> UPushBoxFunctionLibrary::GetAllAtlasColorTextures()
{
	TArray<FSoftObjectPath> Result;
	Result.Reserve(GAtlasTextureCount);
	for (int32 i = 0; i < GAtlasTextureCount; ++i)
	{
		Result.Add(FSoftObjectPath(GAtlasTexturePaths[i]));
	}
	return Result;
}

FSoftObjectPath UPushBoxFunctionLibrary::GetRandomAtlasColorTexture()
{
	const int32 Idx = FMath::RandRange(0, GAtlasTextureCount - 1);
	return FSoftObjectPath(GAtlasTexturePaths[Idx]);
}

bool UPushBoxFunctionLibrary::ApplyAtlasThemeColor(const UObject* WorldContextObject, const FSoftObjectPath& TexturePath)
{
	// Load the texture
	UTexture2D* Texture = Cast<UTexture2D>(TexturePath.TryLoad());
	if (!Texture)
	{
		UE_LOG(LogPushBox, Warning, TEXT("ApplyAtlasThemeColor: Failed to load texture: %s"), *TexturePath.ToString());
		return false;
	}

	// Load the MI_Atlas material instance
	static const TCHAR* MIAtlasPath = TEXT("/Game/Low_Poly_Sci-Fi_Corridor/Materials/MI_Atlas.MI_Atlas");
	UMaterialInstanceConstant* MI = Cast<UMaterialInstanceConstant>(
		StaticLoadObject(UMaterialInstanceConstant::StaticClass(), nullptr, MIAtlasPath));
	if (!MI)
	{
		UE_LOG(LogPushBox, Warning, TEXT("ApplyAtlasThemeColor: Failed to load MI_Atlas material instance"));
		return false;
	}

	// Find the "base color" texture parameter and set it
	// The parameter name in MI_Atlas is typically "base color" or "Base Color"
	// We try common names
	static const FName ParamNames[] = {
		FName(TEXT("base color")),
		FName(TEXT("Base Color")),
		FName(TEXT("BaseColor")),
		FName(TEXT("Base_Color")),
	};

	bool bSet = false;
	for (const FName& ParamName : ParamNames)
	{
		// Check if this parameter exists
		UTexture* CurrentTex = nullptr;
		if (MI->GetTextureParameterValue(ParamName, CurrentTex))
		{
			MI->SetTextureParameterValueEditorOnly(ParamName, Texture);
			bSet = true;
			UE_LOG(LogPushBox, Log, TEXT("ApplyAtlasThemeColor: Set '%s' to '%s'"),
				*ParamName.ToString(), *Texture->GetName());
			break;
		}
	}

	if (!bSet)
	{
		// Fallback: try to set by iterating all texture parameter overrides
		// and replacing the first one that references a T_Atlas texture
		for (FTextureParameterValue& Param : MI->TextureParameterValues)
		{
			if (Param.ParameterValue && Param.ParameterValue->GetName().Contains(TEXT("T_Atlas")))
			{
				MI->SetTextureParameterValueEditorOnly(Param.ParameterInfo.Name, Texture);
				bSet = true;
				UE_LOG(LogPushBox, Log, TEXT("ApplyAtlasThemeColor: Set param '%s' to '%s' (fallback)"),
					*Param.ParameterInfo.Name.ToString(), *Texture->GetName());
				break;
			}
		}
	}

	if (!bSet)
	{
		UE_LOG(LogPushBox, Warning, TEXT("ApplyAtlasThemeColor: Could not find base color parameter on MI_Atlas"));
	}

	return bSet;
}
