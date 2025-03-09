// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryState.h"

#include "InventoryRegistry/CommonInventoryRegistry.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryState)

/************************************************************************/
/* FCommonInventoryItem                                                 */
/************************************************************************/

bool FCommonInventoryItem::Serialize(FArchive& Ar)
{
	return false;
}

bool FCommonInventoryItem::NetSerialize(FArchive& Ar, UPackageMap*, bool& bOutSuccess)
{
	bOutSuccess = false;
	return true;
}

/************************************************************************/
/* FCommonInventoryState                                                */
/************************************************************************/

void FCommonInventoryState::Initialize(uint32 InInitialCapacity)
{
	Items.AddDefaulted(InInitialCapacity);

	for (int32 Idx = 0; Idx < Items.Num(); ++Idx)
	{
		Items[Idx].SetOffset(Idx);
	}
}

void FCommonInventoryState::Deinitialize()
{

}

bool FCommonInventoryState::Serialize(FArchive& Ar)
{
	return false;
}
