// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Logging/StructuredLog.h"

// Custom Category.
COMMONINVENTORY_API DECLARE_LOG_CATEGORY_EXTERN(LogCommonInventory, All, All);

// Logging.
#define COMMON_INVENTORY_LOG(Verb, Fmt, ...)		UE_LOG(LogCommonInventory, Verb, TEXT(Fmt), ##__VA_ARGS__)
#define COMMON_INVENTORY_CLOG(Cond, Verb, Fmt, ...)	UE_CLOG((Cond), LogCommonInventory, Verb, TEXT(Fmt), ##__VA_ARGS__)
#define COMMON_INVENTORY_LOGFMT(Verb, Fmt, ...)		UE_LOGFMT(LogCommonInventory, Verb, Fmt, ##__VA_ARGS__)
