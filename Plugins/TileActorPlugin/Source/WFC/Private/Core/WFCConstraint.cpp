// Copyright Bohdon Sayre. All Rights Reserved.


#include "Core/WFCConstraint.h"

#include "Core/WFCGenerator.h"


void UWFCConstraint::Initialize(UWFCGenerator* InGenerator)
{
	Generator = InGenerator;
	check(Generator != nullptr);

	Grid = Generator->GetGrid();
	check(Grid != nullptr);

	Model = Generator->GetModel();
	check(Model != nullptr);
}

void UWFCConstraint::Reset()
{
}

bool UWFCConstraint::Next()
{
	return false;
}
