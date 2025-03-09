// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "Misc/Build.h"

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

#include "CommonInventoryLog.h"
#include "HAL/IConsoleManager.h"
#include "InventoryRegistry/CommonInventoryRegistry.h"

static void ExecDumpRegistry(const TArray<FString>& InArgs)
{
	if (const UCommonInventoryRegistry* const Registry = UCommonInventoryRegistry::GetPtr())
	{
		if (InArgs.IsEmpty())
		{
			Registry->GetRegistryState().Dump();
		}
		else
		{
			for (const FString& Arg : InArgs)
			{
				const FCommonInventoryRegistryRecord* FoundRecord = nullptr;

				for (const FCommonInventoryRegistryRecord& Record : Registry->GetRegistryRecords())
				{
					if (Record.GetPrimaryAssetName().ToString().Find(Arg, ESearchCase::IgnoreCase) != INDEX_NONE)
					{
						FoundRecord = &Record;
						break;
					}
				}

				if (FoundRecord)
				{
					FoundRecord->Dump();
				}
				else
				{
					COMMON_INVENTORY_LOG(Warning, "Failed to find record for substring: '%s'.", *Arg);
				}
			}
		}
	}
}

static FAutoConsoleCommand CVarRegistryDump(
	TEXT("CommonInventory.DumpRegistry"),
	TEXT("Dumps the internal registry state into the log."),
	FConsoleCommandWithArgsDelegate::CreateStatic(ExecDumpRegistry),
	ECVF_Cheat
);

static void ExecForceRefreshRegistry()
{
	if (UCommonInventoryRegistry* const Registry = UCommonInventoryRegistry::GetPtr())
	{
		Registry->ForceRefresh();
	}
}

static FAutoConsoleCommand CVarRegistryRefresh(
	TEXT("CommonInventory.ForceRefreshRegistry"),
	TEXT("Force refresh the registry state."),
	FConsoleCommandDelegate::CreateStatic(ExecForceRefreshRegistry),
	ECVF_Cheat
);

#endif
