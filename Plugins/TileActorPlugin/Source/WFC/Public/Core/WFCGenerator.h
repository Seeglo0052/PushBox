// Copyright Bohdon Sayre. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WFCTypes.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "Math/RandomStream.h"

#include "WFCGenerator.generated.h"


class UWFCModel;
class UWFCGrid;
class UWFCGridConfig;
class UWFCConstraint;


/**
 * Required objects and settings for initializing a WFCGenerator.
 */
USTRUCT(BlueprintType)
struct FWFCGeneratorConfig
{
	GENERATED_BODY()

	FWFCGeneratorConfig()
	{
	}

	UPROPERTY()
	TWeakObjectPtr<UWFCModel> Model;

	UPROPERTY()
	TWeakObjectPtr<const UWFCGridConfig> GridConfig;

	UPROPERTY()
	TArray<TSubclassOf<UWFCConstraint>> ConstraintClasses;
};


/**
 * Grid-based tile manager. Assigns tiles to cells and manages their state.
 */
UCLASS(BlueprintType, Blueprintable)
class WFC_API UWFCGenerator : public UObject
{
	GENERATED_BODY()

public:
	UWFCGenerator();

	UPROPERTY(BlueprintReadOnly)
	FWFCGeneratorConfig Config;

	UPROPERTY(BlueprintReadOnly)
	EWFCGeneratorState State;

	void SetState(EWFCGeneratorState NewState);

	UFUNCTION(BlueprintPure)
	FORCEINLINE int32 GetNumCells() const { return NumCells; }

	UFUNCTION(BlueprintPure)
	FORCEINLINE int32 GetNumTiles() const { return NumTiles; }

	UFUNCTION(BlueprintPure)
	const UWFCModel* GetModel() const { return Config.Model.Get(); }

	UFUNCTION(BlueprintPure)
	const UWFCGridConfig* GetGridConfig() const { return Config.GridConfig.Get(); }

	template <class T>
	const T* GetModel() const { return Cast<T>(GetModel()); }

	FORCEINLINE const UWFCGrid* GetGrid() const { return Grid; }

	template <class T>
	const T* GetGrid() const { return Cast<T>(GetGrid()); }

	UFUNCTION(BlueprintPure, Meta = (DeterminesOutputType="ConstraintClass"))
	UWFCConstraint* GetConstraint(TSubclassOf<UWFCConstraint> ConstraintClass) const;

	void Configure(FWFCGeneratorConfig InConfig);

	UFUNCTION(BlueprintCallable)
	void Initialize(bool bFull = true);

	UFUNCTION(BlueprintCallable)
	void InitializeConstraints();

	UFUNCTION(BlueprintPure)
	bool IsInitialized() const { return bIsInitialized; }

	UFUNCTION(BlueprintCallable)
	void SetRandomSeed(int32 Seed);

	void SetRandomStream(const FRandomStream& InRandomStream);
	const FRandomStream& GetRandomStream() const { return RandomStream; }

	UFUNCTION(BlueprintCallable)
	void Reset();

	UFUNCTION(BlueprintCallable)
	void Run(int32 StepLimit = 100000);

	bool TryDirectAssignment();

	UFUNCTION(BlueprintCallable)
	void Select(int32 CellIndex, int32 TileId);

	FORCEINLINE bool IsValidCellIndex(FWFCCellIndex Index) const { return Cells.IsValidIndex(Index); }
	FORCEINLINE bool IsValidTileId(FWFCTileId TileId) const { return TileId >= 0 && TileId < NumTiles; }

	FWFCCell& GetCell(FWFCCellIndex CellIndex);
	const FWFCCell& GetCell(FWFCCellIndex CellIndex) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	int32 GetNumCellCandidates(int32 CellIndex) const;

	FORCEINLINE TArray<TObjectPtr<UWFCConstraint>>& GetConstraints() { return Constraints; }
	FORCEINLINE const TArray<TObjectPtr<UWFCConstraint>>& GetConstraints() const { return Constraints; }

	FString GetTileDebugString(int32 TileId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false)
	void GetSelectedTileIds(TArray<int32>& OutTileIds) const;

	const TArray<FWFCCellIndex>& GetCellsAffectedThisUpdate() const { return CellsAffectedThisUpdate; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FCellSelectedDelegate, int32);
	FCellSelectedDelegate OnCellSelected;

	DECLARE_MULTICAST_DELEGATE_OneParam(FStateChangedDelegate, EWFCGeneratorState);
	FStateChangedDelegate OnStateChanged;

	template <class T>
	T* GetConstraint()
	{
		T* Result = nullptr;
		if (Constraints.FindItemByClass(&Result)) return Result;
		return nullptr;
	}

	template <class T>
	const T* GetConstraint() const
	{
		const T* Result = nullptr;
		if (Constraints.FindItemByClass(&Result)) return Result;
		return nullptr;
	}

protected:
	UPROPERTY(Transient)
	TObjectPtr<const UWFCGrid> Grid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWFCConstraint>> Constraints;

	int32 NumTiles;
	TArray<FWFCCell> Cells;
	int32 NumCells;
	int32 NumUnselectedCells;
	bool bIsInitialized;
	TArray<FWFCCellIndex> CellsAffectedThisUpdate;
	FRandomStream RandomStream;
	bool bUseCustomRandomStream;

public:
	bool bForceDeterministic;

protected:
	void InitializeGrid(const UWFCGridConfig* GridConfig);
	void CreateConstraints();
	void InitializeCells();
	void OnCellChanged(FWFCCellIndex CellIndex);
};
