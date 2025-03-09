// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventory.h"
#include "CommonInventoryLog.h"
#include "CommonInventoryTrace.h"

DEFINE_LOG_CATEGORY(LogCommonInventory);
UE_TRACE_CHANNEL_DEFINE(CommonInventoryChannel);
IMPLEMENT_MODULE(FCommonInventoryModule, CommonInventory);
