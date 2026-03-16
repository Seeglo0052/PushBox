// Copyright Bohdon Sayre. All Rights Reserved.


#include "Core/WFCCellSelector.h"

#include "Core/WFCGenerator.h"


// UWFCCellSelector
// ----------------

void UWFCCellSelector::Initialize(UWFCGenerator* InGenerator)
{
	Generator = InGenerator;
}

void UWFCCellSelector::Reset()
{
}

FWFCCellIndex UWFCCellSelector::SelectNextCell()
{
	return INDEX_NONE;
}


// UWFCRandomCellSelector
// ----------------------

FWFCCellIndex UWFCRandomCellSelector::SelectNextCell()
{
	// Simplified: no random cell selection needed
	return INDEX_NONE;
}
