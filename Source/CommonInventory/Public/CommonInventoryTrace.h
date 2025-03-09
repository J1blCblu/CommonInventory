// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Trace/Trace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

// Custom channel declaration. -trace=Cpu,CommonInventory
UE_TRACE_CHANNEL_EXTERN(CommonInventoryChannel, COMMONINVENTORY_API);

// Tracing.
#define COMMON_INVENTORY_SCOPED_TRACE(EventName) TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL(EventName, CommonInventoryChannel)

// Bookmark.
#define COMMON_INVENTORY_BOOKMARK(Name, ...) if constexpr (UE_TRACE_CHANNELEXPR_IS_ENABLED(CommonInventoryChannel)) { TRACE_BOOKMARK(TEXT(Name), ##__VA_ARGS__) }
