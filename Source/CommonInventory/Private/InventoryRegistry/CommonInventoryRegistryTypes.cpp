// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "InventoryRegistry/CommonInventoryRegistryTypes.h"

#include "CommonInventoryLog.h"
#include "CommonInventorySettings.h"
#include "CommonInventoryTrace.h"
#include "CommonInventoryUtility.h"

#include "Algo/BinarySearch.h"
#include "Algo/ForEach.h"
#include "Algo/IsSorted.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/CustomVersion.h"
#include "Serialization/LargeMemoryReader.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/Crc.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/EnumerateRange.h"
#include "Misc/Guid.h"
#include "UObject/UObjectGlobals.h"
 
#if WITH_EDITOR
#include "Misc/ScopeLock.h"
#include "Misc/SpinLock.h"
#endif

#include <type_traits>

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryRegistryTypes)

/************************************************************************/
/* FCommonInventoryRegistryRecord                                       */
/************************************************************************/

bool operator<(const FCommonInventoryRegistryRecord& Lhs, const FCommonInventoryRegistryRecord& Rhs)
{
	if (Lhs.GetPrimaryAssetType() != Rhs.GetPrimaryAssetType())
	{
		return Lhs.GetPrimaryAssetType().LexicalLess(Rhs.GetPrimaryAssetType());
	}

	return Lhs.GetPrimaryAssetName().LexicalLess(Rhs.GetPrimaryAssetName());
}

bool FCommonInventoryRegistryRecord::HasIdenticalData(const FCommonInventoryRegistryRecord& Other) const
{
	if (SharedData == Other.SharedData && AssetPath == Other.AssetPath)
	{
		auto Identical = [](FConstStructView Lhs, FConstStructView Rhs)
			{
				if (Lhs.GetScriptStruct() == Rhs.GetScriptStruct())
				{
					return !Lhs.IsValid() || Lhs.GetScriptStruct()->CompareScriptStruct(Lhs.GetMemory(), Rhs.GetMemory(), PPF_None);
				}

				return false;
			};

		return Identical(DefaultPayload, Other.DefaultPayload) && Identical(CustomData, Other.CustomData);
	}

	return false;
}

uint32 FCommonInventoryRegistryRecord::GetChecksum() const
{
	if (Checksum == 0)
	{
		Checksum = FCrc::StrCrc32(*CommonInventory::ExportScriptStruct(FConstStructView::Make(SharedData)), Checksum);
		Checksum = FCrc::StrCrc32(*CommonInventory::ExportScriptStruct(DefaultPayload), Checksum);
		Checksum = FCrc::StrCrc32(*CommonInventory::ExportScriptStruct(CustomData), Checksum);
		Checksum = FCrc::StrCrc32(*AssetPath.GetAssetPathString(), Checksum);
	}

	return Checksum;
}

void FCommonInventoryRegistryRecord::Dump() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	COMMON_INVENTORY_LOG(Log, "Name: %s, Archetype: %s, Data Source: '%s', RepIndex: %02u, Hash: %#x", *GetPrimaryAssetName().ToString(), *GetPrimaryAssetType().ToString(), *AssetPath.ToString(), RepIndex, Checksum);
	COMMON_INVENTORY_LOG(Log, "Shared Data: %s", *CommonInventory::ExportScriptStruct(FConstStructView::Make(SharedData)));
	COMMON_INVENTORY_LOG(Log, "Default Payload [%d]: %s.", DefaultPayloadIndex, *CommonInventory::ExportScriptStruct(DefaultPayload));
	COMMON_INVENTORY_LOG(Log, "Custom Data [%d]: %s.", CustomDataIndex, *CommonInventory::ExportScriptStruct(CustomData));
#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

/************************************************************************/
/* FCommonInventoryRegistryState                                        */
/************************************************************************/

#if UE_VERSION_OLDER_THAN(5, 5, 0)

COMMON_INVENTORY_IMPLEMENT_GET_PRIVATE_MEMBER(FInstancedStructContainer, NumItems, int32);
FCommonInventoryRegistryState& FCommonInventoryRegistryState::operator=(FCommonInventoryRegistryState&& Other)
{
	if (ensure(this != &Other))
	{
		const int32 CustomDataOriginalNum = Other.CustomDataContainer.Num();

		DataContainer = MoveTemp(Other.DataContainer);
		CustomDataContainer = MoveTemp(Other.CustomDataContainer);
		Archetypes = MoveTemp(Other.Archetypes);
		DataMap = MoveTemp(Other.DataMap);
		ReplicationMap = MoveTemp(Other.ReplicationMap);

		RepIndexEncodingBitsNum = Other.RepIndexEncodingBitsNum;
		Checksum = Other.Checksum;

		Other.RepIndexEncodingBitsNum = 0;
		Other.Checksum = 0;

		COMMON_INVENTORY_GET_PRIVATE_MEMBER(FInstancedStructContainer, CustomDataContainer, NumItems) = CustomDataOriginalNum;
	}
	
	return *this;
}

#endif // UE_VERSION_OLDER_THAN(5, 5, 0)

void FCommonInventoryRegistryState::Reset(TConstArrayView<FCommonInventoryRegistryRecord> InRegistryData)
{
	DataContainer.Reset(InRegistryData.Num());
	DataContainer.Append(InRegistryData);
	DataContainer.Sort();

	TArray<FConstStructView, TInlineAllocator<128>> CustomData;
	CustomData.Reserve(DataContainer.Num() * 2);

	auto CollectCustomData = [&CustomData](FConstStructView InStructView, int32& OutIndex)
		{
			OutIndex = InStructView.IsValid() ? CustomData.Emplace(InStructView) : INDEX_NONE;
		};

	// FixupDependencies() will update DefaultPayload and CustomData views.
	Algo::ForEach(DataContainer, [&](FCommonInventoryRegistryRecord& Record) { CollectCustomData(Record.DefaultPayload, Record.DefaultPayloadIndex); });
	Algo::ForEach(DataContainer, [&](FCommonInventoryRegistryRecord& Record) { CollectCustomData(Record.CustomData, Record.CustomDataIndex); });

	CustomDataContainer.Reset();
	CustomDataContainer.Append(CustomData);

	// Invalidate cached checksums just in case.
	Algo::ForEach(DataContainer, &FCommonInventoryRegistryRecord::InvalidateChecksum);

	// Finally fixup secondary data.
	FixupDependencies(/* bMigrateArchetypeChecksum */ false);
}

bool FCommonInventoryRegistryState::AppendData(const FCommonInventoryRegistryRecord& InRecord)
{
	// Try to update existing data.
	if (ContainsRecord(InRecord.GetPrimaryAssetId()))
	{
		FCommonInventoryRegistryRecord& ExistingRecord = DataContainer[DataMap.FindChecked(InRecord.GetPrimaryAssetId())];
		ExistingRecord.SharedData = InRecord.SharedData;
		ExistingRecord.AssetPath = InRecord.AssetPath;

		auto RefreshCustomData = [this](FConstStructView InStructView, FConstStructView& ExistingStructView, int32& TargetIndex)
			{
				if (ExistingStructView.IsValid() && ensure(TargetIndex != INDEX_NONE))
				{
					// Just copy custom data if the type matches.
					if (ExistingStructView.GetScriptStruct() == InStructView.GetScriptStruct())
					{
						const uint8* const Source = InStructView.GetMemory();
						uint8* const Dest = CustomDataContainer[TargetIndex].GetMemory();
						ExistingStructView.GetScriptStruct()->CopyScriptStruct(Dest, Source);
						return;
					}
					else
					{
						RemoveCustomData(TargetIndex);
					}
				}

				if (InStructView.IsValid())
				{
					TargetIndex = CustomDataContainer.Num();
					CustomDataContainer.Append({ InStructView });
					ExistingStructView = CustomDataContainer[TargetIndex];
				}
				else
				{
					// Reset previous data.
					TargetIndex = INDEX_NONE;
					ExistingStructView.Reset();
				}
			};

		RefreshCustomData(InRecord.DefaultPayload, ExistingRecord.DefaultPayload, ExistingRecord.DefaultPayloadIndex);
		RefreshCustomData(InRecord.CustomData, ExistingRecord.CustomData, ExistingRecord.CustomDataIndex);
		
		// Invalidate the entire checksum chain.
		FindArchetypeGroup(ExistingRecord.GetPrimaryAssetType())->Checksum = 0;
		ExistingRecord.InvalidateChecksum();
		Checksum = 0;

		return false;
	}

	// Insert a new record.
	const int32 RecordIndex = Algo::LowerBound(DataContainer, InRecord);
	FCommonInventoryRegistryRecord& NewRecord = DataContainer.Insert_GetRef(InRecord, RecordIndex);

	// Invalidate archetype checksum.
	if (FArchetypeGroup* const Archetype = FindArchetypeGroup(NewRecord.GetPrimaryAssetType()))
	{
		Archetype->Checksum = 0;
	}

	// Invalidate cached checksum just in case.
	NewRecord.InvalidateChecksum();

	auto AppendCustomData = [this](FConstStructView InStructView, int32& OutIndex)
		{
			if (InStructView.IsValid())
			{
				// FixupDependencies() will update views.
				OutIndex = CustomDataContainer.Num();
				CustomDataContainer.Append({ InStructView });
			}
			else
			{
				OutIndex = INDEX_NONE; // Just to be safe...
			}
		};

	AppendCustomData(InRecord.DefaultPayload, NewRecord.DefaultPayloadIndex);
	AppendCustomData(InRecord.CustomData, NewRecord.CustomDataIndex);
	FixupDependencies(/* bMigrateArchetypeChecksum */ true);

	return true;
}

bool FCommonInventoryRegistryState::RemoveData(FPrimaryAssetId PrimaryAssetId)
{
	if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRecordPtr(PrimaryAssetId))
	{
		// Remove DefaultPayload.
		if (RegistryRecord->DefaultPayload.IsValid())
		{
			check(RegistryRecord->DefaultPayloadIndex != INDEX_NONE);
			RemoveCustomData(RegistryRecord->DefaultPayloadIndex);
		}

		// Remove CustomData.
		if (RegistryRecord->CustomData.IsValid())
		{
			check(RegistryRecord->CustomDataIndex != INDEX_NONE);
			RemoveCustomData(RegistryRecord->CustomDataIndex);
		}

		// Invalidate the archetype checksum.
		FindArchetypeGroup(RegistryRecord->GetPrimaryAssetType())->Checksum = 0;

		// Finally, remove record and fixup secondary data.
		const int32 Idx = DataMap.FindChecked(RegistryRecord->GetPrimaryAssetId());
		DataContainer.RemoveAt(Idx, EAllowShrinking::No);
		FixupDependencies(/* bMigrateArchetypeChecksum */ true);

		return true;
	}

	return false;
}

void FCommonInventoryRegistryState::FixupDependencies(bool bMigrateArchetypeChecksum /* = false */)
{
	check(Algo::IsSorted(DataContainer));

	RepIndexEncodingBitsNum = FMath::CeilLogTwo(DataContainer.Num() + /* Invalid */ 1);
	ReplicationMap.Empty(DataContainer.Num());
	DataMap.Empty(DataContainer.Num());
	
	// Keep archetype data to migrate checksum later.
	TArray<FArchetypeGroup, TInlineAllocator<12>> CachedArchetypes = MoveTemp(Archetypes);

	// Used to iteratively populate archetype data.
	FArchetypeGroup* ArchetypeIterator = nullptr;

	for (TEnumerateRef<FCommonInventoryRegistryRecord> RegistryData : EnumerateRange(DataContainer))
	{
		checkf(!DataMap.Contains(RegistryData->GetPrimaryAssetId()), TEXT("FInventoryRegistryState: Found duplicated FPrimaryAssetId."));

		// Refresh mappings.
		RegistryData->RepIndex = RegistryData.GetIndex() + /* Invalid */ 1;
		ReplicationMap.Add(RegistryData->RepIndex, RegistryData.GetIndex());
		DataMap.Add(RegistryData->GetPrimaryAssetId(), RegistryData.GetIndex());

		auto RefreshViewData = [this](FConstStructView& OutStructView, int32 InIndex)
			{
				if (CustomDataContainer.IsValidIndex(InIndex))
				{
					OutStructView = CustomDataContainer[InIndex];
				}
				else
				{
					OutStructView.Reset();
				}
			};

		// Refresh views.
		RefreshViewData(RegistryData->DefaultPayload, RegistryData->DefaultPayloadIndex);
		RefreshViewData(RegistryData->CustomData, RegistryData->CustomDataIndex);

		// Refresh archetype groups.
		if (!ArchetypeIterator || ArchetypeIterator->PrimaryAssetType != RegistryData->GetPrimaryAssetType())
		{
			ArchetypeIterator = &Archetypes.Emplace_GetRef(RegistryData->GetPrimaryAssetType(), RegistryData.GetIndex());
		}

		++ArchetypeIterator->Offset;
	}

	Checksum = 0; // Invalidate the top-level checksum.

	if (bMigrateArchetypeChecksum)
	{
		for (FArchetypeGroup& Archetype : Archetypes)
		{
			if (const FArchetypeGroup* const PreviousArchetype = Algo::FindBy(CachedArchetypes, Archetype.PrimaryAssetType, &FArchetypeGroup::PrimaryAssetType))
			{
				Archetype.Checksum = PreviousArchetype->Checksum;
			}
		}
	}
}

void FCommonInventoryRegistryState::RemoveCustomData(int32 InCustomDataIndex)
{
	if (CustomDataContainer.IsValidIndex(InCustomDataIndex))
	{
		CustomDataContainer.RemoveAt(InCustomDataIndex, /* Count */ 1);

		// Shift subsequent indices and update cached views.
		if (CustomDataContainer.IsValidIndex(InCustomDataIndex))
		{
			// DefaultPayload and CustomData shares the same container.
			for (FCommonInventoryRegistryRecord& RegistryRecord : DataContainer)
			{
				if (RegistryRecord.DefaultPayloadIndex > InCustomDataIndex)
				{
					RegistryRecord.DefaultPayload = CustomDataContainer[--RegistryRecord.DefaultPayloadIndex];
				}

				if (RegistryRecord.CustomDataIndex > InCustomDataIndex)
				{
					RegistryRecord.CustomData = CustomDataContainer[--RegistryRecord.CustomDataIndex];
				}
			}
		}
	}
}

uint32 FCommonInventoryRegistryState::GetChecksum() const
{
	if (Checksum == 0)
	{
		for (const FArchetypeGroup& Archetype : Archetypes)
		{
			if (Archetype.Checksum == 0)
			{
				for (const FCommonInventoryRegistryRecord& Record : MakeArrayView(DataContainer.GetData() + Archetype.Begin, Archetype.Offset))
				{
					Archetype.Checksum = FCrc::TypeCrc32(Record.GetChecksum(), Archetype.Checksum);
				}
			}

			Checksum = FCrc::TypeCrc32(Archetype.Checksum, Checksum);
		}
	}

	return Checksum;
}

struct FInventoryRegistryHeaderVersion
{
	FInventoryRegistryHeaderVersion() = delete;

	// "Common Inventory" little endian hex representation.
	static constexpr FGuid Magic{ 0x6D6D6F43 , 0x49206E6F, 0x6E65766E, 0x79726F74 };

	enum Type : uint32
	{
		InitialVersion = 0,

		// -----<new versions can be added above this line>-----
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
};

struct FInventoryRegistryHeader
{
	static inline const FName ArchiveName = TEXT("InventoryRegistryArchive");

	/** Header version. */
	FInventoryRegistryHeaderVersion::Type Version = FInventoryRegistryHeaderVersion::LatestVersion;

	/** The associated data source class. */
	FSoftClassPath DataSourceClass;

	/** List of custom versions registered during saving. */
	FCustomVersionContainer VersionContainer;

	/** Final checksum of the data, excluding the header. */
	uint32 Checksum = 0;

	/** Whether the editor only data should be stripped. */
	bool bIsCooked = false;

public:

	bool Serialize(FArchive& Ar)
	{
		FGuid Magic;

		if (Ar.IsSaving())
		{
			Magic = FInventoryRegistryHeaderVersion::Magic;
		}

		Ar << Magic;

		if (Ar.IsLoading() && Magic != FInventoryRegistryHeaderVersion::Magic)
		{
			return false;
		}

		std::underlying_type_t<decltype(Version)> VersionValue = Version;
		Ar << VersionValue << Checksum << bIsCooked;
		DataSourceClass.SerializePathWithoutFixup(Ar);
		VersionContainer.Serialize(Ar);
		Version = static_cast<decltype(Version)>(VersionValue);

		return !Ar.IsError();
	}
};

bool FCommonInventoryRegistryState::SaveToFile(const FString& Filename, bool bIsCooking)
{
	if (TUniquePtr<FArchive> Writer{ IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_EvenIfReadOnly) })
	{
		FNameAsStringProxyArchive ProxyWriter(*Writer);
		return SaveState(ProxyWriter, bIsCooking);
	}

	return false;
}

bool FCommonInventoryRegistryState::SaveState(FArchive& Ar, bool bIsCooking)
{
	check(Ar.IsSaving());

	// In-memory storage for serialized state.
	FBufferArchive64 BufferArchive(/* bIsPersistent */ true, FInventoryRegistryHeader::ArchiveName);
	BufferArchive.SetWantBinaryPropertySerialization(bIsCooking);
	FObjectAndNameAsStringProxyArchive Writer(BufferArchive, false);
	Writer.SetFilterEditorOnly(bIsCooking);
	
	// Write the actual state.
	StaticStruct()->SerializeItem(Writer, this, nullptr);

	if (BufferArchive.IsError())
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Failed to serialize state.");
		return false;
	}

	// Fill header information.
	FInventoryRegistryHeader Header
	{
		.Version = FInventoryRegistryHeaderVersion::LatestVersion,
		.DataSourceClass = UCommonInventorySettings::Get()->DataSourceClassName,
		.VersionContainer = Writer.GetCustomVersions(), // Copy all registered custom versions.
		.Checksum = FCrc::MemCrc32(BufferArchive.GetData(), BufferArchive.Num()), // xxhash
		.bIsCooked = bIsCooking
	};

	// Write header on disk.
	if (!Header.Serialize(Ar))
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Failed to serialize header.");
		return false;
	}
	
	// Stage the serialized state on disk. Compression is performed as part of the .pak file.
	Ar.Serialize(BufferArchive.GetData(), BufferArchive.Num());

	return !Ar.IsError();
}

bool FCommonInventoryRegistryState::LoadFromFile(const FString& Filename, bool bIsCooked)
{
	if (IFileManager::Get().FileExists(*Filename))
	{
		if (TUniquePtr<FArchive> Reader{ IFileManager::Get().CreateFileReader(*Filename, FILEREAD_None) })
		{
			FNameAsStringProxyArchive ProxyReader(*Reader);
			return LoadState(ProxyReader, bIsCooked);
		}
	}

	return false;
}

bool FCommonInventoryRegistryState::LoadState(FArchive& Ar, bool bIsCooked)
{
	check(Ar.IsLoading());

	// Read the header first.
	FInventoryRegistryHeader Header;

	if (!Header.Serialize(Ar))
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Failed to read header in InventoryRegistry.bin.");
		return false;
	}

	if (Header.Version > FInventoryRegistryHeaderVersion::LatestVersion)
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Unable to load InventoryRegistry.bin with a newer version.");
		return false;
	}

	if (Header.DataSourceClass != UCommonInventorySettings::Get()->DataSourceClassName)
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Unable to load InventoryRegistry.bin with different data source.");
		return false;
	}

	if (Header.bIsCooked != bIsCooked)
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Unable to load InventoryRegistry.bin with unexpected cooking state.");
		return false;
	}

	// Load the remaining file into memory.
	TArray64<uint8> SerializedState;
	SerializedState.SetNumUninitialized(Ar.TotalSize() - Ar.Tell());
	Ar.Serialize(SerializedState.GetData(), SerializedState.Num());

	if (Ar.IsError())
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Failed to load InventoryRegistry.bin into memory.");
		return false;
	}

	const uint32 DataChecksum = FCrc::MemCrc32(SerializedState.GetData(), SerializedState.Num());

	if (DataChecksum != Header.Checksum)
	{
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Failed to verify data integrity for InventoryRegistry.bin.");
		return false;
	}

	FLargeMemoryReader MemoryReader(SerializedState.GetData(), SerializedState.Num(), ELargeMemoryReaderFlags::Persistent, FInventoryRegistryHeader::ArchiveName);
	MemoryReader.SetCustomVersions(Header.VersionContainer);
	MemoryReader.SetWantBinaryPropertySerialization(bIsCooked);
	FObjectAndNameAsStringProxyArchive Reader(MemoryReader, true); // Load UUserDefinedStruct* if needed.
	Reader.SetFilterEditorOnly(bIsCooked);

	StaticStruct()->SerializeItem(Reader, this, /* Defaults */ nullptr);

	if (!Reader.AtEnd() || Reader.IsError())
	{
		Reset();
		COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Failed to read data from InventoryRegistry.bin.");
		return false;
	}

	for (const FConstStructView CustomData : CustomDataContainer)
	{
		if (!CustomData.IsValid())
		{
			Reset();
			COMMON_INVENTORY_LOG(Error, "FCommonInventoryRegistryState: Found unsupported or missed DefaultPayload/CustomData type in InventoryRegistry.bin.");
			return false;
		}
	}

	// Restore secondary data.
	FixupDependencies();

	return true;
}

void FCommonInventoryRegistryState::DiffRecords(const FCommonInventoryRegistryState& InBaseState)
{
	if (HasRecords() && InBaseState.HasRecords())
	{
		const FCommonInventoryRegistryState* FirstState = this;
		const FCommonInventoryRegistryState* SecondState = &InBaseState;

		// Enumerate the registry with fewer records.
		if (FirstState->GetRecordsNum() > SecondState->GetRecordsNum())
		{
			Swap(FirstState, SecondState);
		}

		bool (FCommonInventoryRegistryRecord::*Identical)(const FCommonInventoryRegistryRecord&) const = &FCommonInventoryRegistryRecord::HasIdenticalData;

		// If possible, try using a faster version.
		if (Checksum != 0 && InBaseState.Checksum != 0)
		{
			Identical = &FCommonInventoryRegistryRecord::HasIdenticalDataFast;
		}

		TArray<FPrimaryAssetId, TInlineAllocator<256>> PendingRemoveRecords;

		for (const FCommonInventoryRegistryRecord& FirstRecord : FirstState->GetRecords())
		{
			if (const FCommonInventoryRegistryRecord* const SecondRecord = SecondState->GetRecordPtr(FirstRecord.GetPrimaryAssetId()))
			{
				if ((SecondRecord->*Identical)(FirstRecord))
				{
					PendingRemoveRecords.Emplace(FirstRecord.GetPrimaryAssetId());
				}
			}
		}

		// Per element removal could be very slow.
		if (PendingRemoveRecords.Num() == GetRecordsNum())
		{
			Reset();
		}
		else
		{
			Algo::ForEach(PendingRemoveRecords, [this](FPrimaryAssetId InPrimaryAssetId) { verify(RemoveData(InPrimaryAssetId)); });
		}
	}
}

void FCommonInventoryRegistryState::Dump() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	COMMON_INVENTORY_LOG(Log, "<=================== Inventory Registry Summary ===================>");
	COMMON_INVENTORY_LOG(Log, "Records: %d, Archetypes: %d, RepIndexEncodingBitsNum: %d, Hash: %#x.", DataContainer.Num(), Archetypes.Num(), RepIndexEncodingBitsNum, Checksum);

	for (const FArchetypeGroup& Archetype : Archetypes)
	{
		COMMON_INVENTORY_LOG(Log, " ");
		COMMON_INVENTORY_LOG(Log, "<===== FPrimaryAssetType: '%s' (%#x) =====>", *Archetype.PrimaryAssetType.ToString(), Archetype.Checksum);

		int32 MaxNameLength = 0;
		TArrayView RecordsRange = GetRecords(Archetype.PrimaryAssetType);

		Algo::ForEach(RecordsRange, [&](const FCommonInventoryRegistryRecord& InRecord)
		{
			MaxNameLength = FMath::Max(MaxNameLength, int32(InRecord.GetPrimaryAssetName().GetStringLength()));
		});

		for (TConstEnumerateRef<FCommonInventoryRegistryRecord> RegistryData : EnumerateRange(RecordsRange))
		{
			COMMON_INVENTORY_LOG(Log, "#%02d: %-*s, RepIndex: %02u, AssetPath: '%s'.",
								 RegistryData.GetIndex(),
								 MaxNameLength, *RegistryData->GetPrimaryAssetName().ToString(),
								 RegistryData->RepIndex,
								 *RegistryData->AssetPath.GetLongPackageName());
		}
	}

	COMMON_INVENTORY_LOG(Log, " ");
	COMMON_INVENTORY_LOG(Log, "<================= Inventory Registry Summary End =================>");

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
}

/************************************************************************/
/* FCommonInventoryRedirects                                            */
/************************************************************************/

#if WITH_EDITOR
// Redirects are persistent in standalone.
static UE::FSpinLock RedirectsCriticalSection;
#endif

template<typename T, int32 NumInlineElements>
using TInlineSet = TSet<T, DefaultKeyFuncs<T>, TInlineSetAllocator<NumInlineElements>>;

static TMap<FName, FName> GenerateRedirectionMap(TArrayView<const FCommonInventoryRedirector> InRedirects)
{
	TMap<FName, FName> RedirectionMap;
	RedirectionMap.Reserve(InRedirects.Num());

	for (const FCommonInventoryRedirector& Redirector : InRedirects)
	{
		if (!Redirector.IsValid())
		{
			COMMON_INVENTORY_LOG(Warning, "Found invalid redirector: %s -> %s.", *Redirector.OldValue.ToString(), *Redirector.NewValue.ToString());
			continue;
		}

		if (RedirectionMap.Contains(Redirector.OldValue))
		{
			COMMON_INVENTORY_LOG(Warning, "Found ambiguous redirector: %s -> %s.", *Redirector.OldValue.ToString(), *Redirector.NewValue.ToString());
			continue;
		}

		RedirectionMap.Emplace(Redirector.OldValue, Redirector.NewValue);
	}

	return RedirectionMap;
}

// Resolves a value with an optional stop value.
static bool ResolveValue(const TMap<FName, FName>& UnresolvedRedirects, const FName InValue, FName& OutValue, const FName* StopValue = nullptr)
{
	TInlineSet<FName, 8> VisitedKeys = { OutValue = InValue };

	while (const FName* const NextValue = UnresolvedRedirects.Find(OutValue))
	{
		if (StopValue && *StopValue == *NextValue)
		{
			OutValue = *StopValue;
			break;
		}

		if (VisitedKeys.Contains(*NextValue))
		{
			COMMON_INVENTORY_LOG(Error, "Failed to collapse redirector '%s -> %s' due to circular dependency.", *InValue.ToString(), *OutValue.ToString());
			OutValue = InValue;
			break;
		}

		VisitedKeys.Emplace(OutValue);
		OutValue = *NextValue;
	}

	return InValue != OutValue;
}

// Resolves a raw redirection map
static void ResolveRedirectionMap(const TMap<FName, FName>& UnresolvedRedirects, TMap<FName, FName>& OutResolvedRedirects)
{
	OutResolvedRedirects.Empty(UnresolvedRedirects.Num());
	TInlineSet<FName, 16> InvalidKeys; // Global dirty list.
	TInlineSet<FName, 16> VisitedKeys; // Local visited list.

	for (auto [OldValue, NewValue] : UnresolvedRedirects)
	{
		// Reuse cached result.
		if (const FName* const ResolvedValue = OutResolvedRedirects.Find(NewValue))
		{
			OutResolvedRedirects.Emplace(OldValue, *ResolvedValue);
			continue;
		}

		// No need to check for OldValue.
		if (InvalidKeys.Contains(NewValue))
		{
			InvalidKeys.Emplace(OldValue);
			continue;
		}

		VisitedKeys.Emplace(OldValue);

		while (const FName* const NextValue = UnresolvedRedirects.Find(NewValue))
		{
			VisitedKeys.Emplace(NewValue);

			if (const FName* const ResolvedValue = OutResolvedRedirects.Find(*NextValue))
			{
				NewValue = *ResolvedValue;
				break;
			}

			if (VisitedKeys.Contains(*NextValue) || InvalidKeys.Contains(*NextValue))
			{
				NewValue = NAME_None;
				break;
			}

			NewValue = *NextValue;
		}

		if (!NewValue.IsNone())
		{
			for (const FName Key : VisitedKeys)
			{
				OutResolvedRedirects.Emplace(Key, NewValue);
			}
		}
		else
		{
			InvalidKeys.Append(VisitedKeys);
		}

		VisitedKeys.Reset();
	}

	COMMON_INVENTORY_CLOG(!InvalidKeys.IsEmpty(), Error, "Failed to fully resolve redirection map due to circular dependencies between %s.",
						  *FString::JoinBy(InvalidKeys, TEXT(", "), UE_PROJECTION_MEMBER(FName, ToString)));
}

static void LoadRedirectsFromConfig(TMap<FName, FName>& OutTypeRedirects, TMap<FName, FName>& OutNameRedirects)
{
	check(IsInGameThread());

	TMap<FName, FName> TypeRedirects, NameRedirects;
	const UCommonInventorySettings* const Settings = UCommonInventorySettings::Get();
	ResolveRedirectionMap(GenerateRedirectionMap(Settings->PrimaryAssetTypeRedirects), TypeRedirects);
	ResolveRedirectionMap(GenerateRedirectionMap(Settings->PrimaryAssetNameRedirects), NameRedirects);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	OutTypeRedirects = MoveTemp(TypeRedirects);
	OutNameRedirects = MoveTemp(NameRedirects);
}

FCommonInventoryRedirects& FCommonInventoryRedirects::Get()
{
	static FCommonInventoryRedirects Instance;
	return Instance;
}

FCommonInventoryRedirects::FCommonInventoryRedirects()
{
	LoadRedirectsFromConfig(TypeRedirectionMap, NameRedirectionMap);
}

#if WITH_EDITOR

void FCommonInventoryRedirects::ForceRefresh()
{
	if (GIsEditor && !GIsPlayInEditorWorld)
	{
		LoadRedirectsFromConfig(TypeRedirectionMap, NameRedirectionMap);
	}
}

#endif // WITH_EDITOR

bool FCommonInventoryRedirects::IsStale(FPrimaryAssetType InPrimaryAssetType) const
{
	const uint32 TypeHash = decltype(TypeRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetType);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	return TypeRedirectionMap.ContainsByHash(TypeHash, InPrimaryAssetType);
}

bool FCommonInventoryRedirects::IsStale(FPrimaryAssetId InPrimaryAssetId) const
{
	const uint32 TypeHash = decltype(TypeRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetId.PrimaryAssetType);
	const uint32 NameHash = decltype(NameRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetId.PrimaryAssetName);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	return NameRedirectionMap.ContainsByHash(NameHash, InPrimaryAssetId.PrimaryAssetName)
		|| TypeRedirectionMap.ContainsByHash(TypeHash, InPrimaryAssetId.PrimaryAssetType);
}

bool FCommonInventoryRedirects::TryRedirect(FPrimaryAssetType& InPrimaryAssetType) const
{
	const uint32 TypeHash = decltype(TypeRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetType);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	if (const FName* const NewValue = TypeRedirectionMap.FindByHash(TypeHash, InPrimaryAssetType))
	{
		InPrimaryAssetType = *NewValue;
		return true;
	}

	return false;
}

bool FCommonInventoryRedirects::TryRedirect(FPrimaryAssetId& InPrimaryAssetId) const
{
	bool bIsDirty = false;
	const uint32 TypeHash = decltype(TypeRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetId.PrimaryAssetType);
	const uint32 NameHash = decltype(NameRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetId.PrimaryAssetName);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	if (const FName* const NewValue = NameRedirectionMap.FindByHash(NameHash, InPrimaryAssetId.PrimaryAssetName))
	{
		InPrimaryAssetId.PrimaryAssetName = *NewValue;
		bIsDirty = true;
	}

	if (const FName* const NewValue = TypeRedirectionMap.FindByHash(TypeHash, InPrimaryAssetId.PrimaryAssetType))
	{
		InPrimaryAssetId.PrimaryAssetType = *NewValue;
		bIsDirty = true;
	}

	return bIsDirty;
}

bool FCommonInventoryRedirects::HasTypeRedirects(FPrimaryAssetType InPrimaryAssetType) const
{
	const uint32 TypeHash = decltype(TypeRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetType);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	return TypeRedirectionMap.ContainsByHash(TypeHash, InPrimaryAssetType) || Algo::FindBy(TypeRedirectionMap, InPrimaryAssetType, &TTuple<FName, FName>::Value) != nullptr;
}

bool FCommonInventoryRedirects::HasNameRedirects(FName InPrimaryAssetName) const
{
	const uint32 NameHash = decltype(NameRedirectionMap)::KeyFuncsType::GetKeyHash(InPrimaryAssetName);

#if	WITH_EDITOR
	const UE::TScopeLock Lock(RedirectsCriticalSection);
#endif

	return NameRedirectionMap.ContainsByHash(NameHash, InPrimaryAssetName) || Algo::FindBy(NameRedirectionMap, InPrimaryAssetName, &TTuple<FName, FName>::Value) != nullptr;
}

void FCommonInventoryRedirects::TraversePermutations(FPrimaryAssetId InPrimaryAssetId, TFunctionRef<bool(FPrimaryAssetId)> InPredicate) const
{
	check(IsInGameThread());

	// First, traverse names with the given type.
	for (const auto [OldName, NewName] : NameRedirectionMap)
	{
		if (NewName == InPrimaryAssetId.PrimaryAssetName)
		{
			if (!InPredicate({ InPrimaryAssetId.PrimaryAssetType, OldName }))
			{
				return;
			}
		}
	}

	// Then, traverse types with the given name.
	for (const auto [OldType, NewType] : TypeRedirectionMap)
	{
		if (NewType == InPrimaryAssetId.PrimaryAssetType)
		{
			if (!InPredicate({ OldType, InPrimaryAssetId.PrimaryAssetName }))
			{
				return;
			}
		}
	}

	// Finally, permute types and names.
	for (const auto [OldType, NewType] : TypeRedirectionMap)
	{
		if (NewType == InPrimaryAssetId.PrimaryAssetType)
		{
			for (auto [OldName, NewName] : NameRedirectionMap)
			{
				if (NewName == InPrimaryAssetId.PrimaryAssetName)
				{
					if (!InPredicate({ OldType, OldName }))
					{
						return;
					}
				}
			}
		}
	}
}

bool FCommonInventoryRedirects::CanResolveInto(TArrayView<const FCommonInventoryRedirector> InRedirects, FName OriginalValue, FName TargetValue)
{
	if (OriginalValue != TargetValue)
	{
		const TMap<FName, FName> UnresolvedRedirects = GenerateRedirectionMap(InRedirects);
		FName ResolvedValue;

		if (ResolveValue(UnresolvedRedirects, OriginalValue, ResolvedValue, &TargetValue))
		{
			return ResolvedValue == TargetValue;
		}
	}

	return false;
}

bool FCommonInventoryRedirects::HasCommonBase(TArrayView<const FCommonInventoryRedirector> InRedirects, FName FirstValue, FName SecondValue)
{
	const TMap<FName, FName> UnresolvedRedirects = GenerateRedirectionMap(InRedirects);
	FName ResolvedFirst, ResolvedSecond;

	if (ResolveValue(UnresolvedRedirects, FirstValue, ResolvedFirst))
	{
		if (ResolvedFirst == SecondValue)
		{
			return true;
		}

		if (ResolveValue(UnresolvedRedirects, SecondValue, ResolvedSecond))
		{
			return ResolvedFirst == ResolvedSecond;
		}
	}

	return false;
}

bool FCommonInventoryRedirects::AppendRedirects(TArray<FCommonInventoryRedirector>& InRedirects, FName OldValue, FName NewValue, bool bInvertCircularDependency)
{
	// There's still possible the following:
	//  (Name1 -> Name2 -> Name3) + (Name10 -> Name3) => (Name1 -> Name2 -> Name3 <- Name10)
	// But this is only possible if Name3 is stale, and I don't think it should be disallowed.

	//  (Name1 -> Name1)
	if (OldValue == NewValue)
	{
		COMMON_INVENTORY_LOG(Warning, "Failed to insert self-redirector '%s' -> '%s'.", *OldValue.ToString(), *NewValue.ToString());
		return false;
	}

	TMap<FName, FName> UnresolvedRedirects = GenerateRedirectionMap(InRedirects);

	//  (Name1 -> Name2 -> Name3) + (Name1 -> Name7)
	if (UnresolvedRedirects.Contains(OldValue))
	{
		COMMON_INVENTORY_LOG(Warning, "Failed to insert ambiguous redirector '%s' -> '%s'.", *OldValue.ToString(), *NewValue.ToString());
		return false;
	}

	//  (Name1 -> Name2 -> Name3) + [(Name3 -> Name4), (Name100 + Name101)]
	if (!UnresolvedRedirects.Contains(NewValue))
	{
		InRedirects.Emplace(OldValue, NewValue);
		return true;
	}

	if (bInvertCircularDependency)
	{
		FName NewValueResolved;

		//  (-> Name1 -> Name2 -> Name3 ->) + (Name100 -> Name2)
		if (!ResolveValue(UnresolvedRedirects, NewValue, NewValueResolved))
		{
			COMMON_INVENTORY_LOG(Warning, "Failed to insert redirector '%s' -> '%s'. Concatenation of redirects into circular dependency is not allowed.", *OldValue.ToString(), *NewValue.ToString());
			return false;
		}

		//  [(Name1 -> Name2 -> Name3), (Name4 -> Name5)] + (Name3 -> Name4)
		if (NewValueResolved != OldValue)
		{
			COMMON_INVENTORY_LOG(Warning, "Failed to insert redirector '%s' -> '%s'. Concatenation of redirects is not allowed.", *OldValue.ToString(), *NewValue.ToString());
			return false;
		}

		//  (Name1 -> Name2 -> Name3 -> Name4) + (Name4 -> Name2) => (Name1 -> Name2 <- Name3 <- Name4)
		for (FName Current = NewValue; const FName* const Next = UnresolvedRedirects.Find(Current); Current = *Next)
		{
			InRedirects[InRedirects.Find({ Current, *Next })].SwapValues();
		}

		return true;
	}

	COMMON_INVENTORY_LOG(Error, "Failed to insert redirector '%s' -> '%s'. Concatenation of redirects is not allowed.", *OldValue.ToString(), *NewValue.ToString());
	return false;
}

bool FCommonInventoryRedirects::CleanupRedirects(TArray<FCommonInventoryRedirector>& InRedirects, const FName TargetValue, bool bRecursively)
{
	TInlineSet<FName, 16> CleanupKeys = { TargetValue };

	if (bRecursively)
	{
		const TMap<FName, FName> UnresolvedRedirects = GenerateRedirectionMap(InRedirects);
		TInlineSet<FName, 16> InvalidKeys;
		TInlineSet<FName, 16> VisitedKeys;

		for (const auto [OldValue, NewValue] : UnresolvedRedirects)
		{
			FName CurrentValue = OldValue;
			const FName* NextValue = &NewValue;

			do
			{
				if (VisitedKeys.Contains(CurrentValue) || InvalidKeys.Contains(CurrentValue))
				{
					break;
				}

				if (CleanupKeys.Contains(CurrentValue))
				{
					CurrentValue = TargetValue;
					break;
				}

				VisitedKeys.Emplace(CurrentValue);
				CurrentValue = *NextValue;
			} while ((NextValue = UnresolvedRedirects.Find(CurrentValue)) != nullptr);

			if (CurrentValue == TargetValue)
			{
				CleanupKeys.Append(VisitedKeys);
			}
			else
			{
				InvalidKeys.Append(VisitedKeys);
			}

			VisitedKeys.Reset();
		}
	}

	return InRedirects.RemoveAll([&CleanupKeys](const FCommonInventoryRedirector& Redirect) { return CleanupKeys.Contains(Redirect.OldValue); }) > 0;
}

/************************************************************************/
/* FCommonInventoryDefaultsPropagator                                   */
/************************************************************************/

struct FCommonInventoryDefaultsPropagationArchive final : public FArchiveUObject
{
	FCommonInventoryDefaultsPropagator::FContext& Context;

public:

	FCommonInventoryDefaultsPropagationArchive(FCommonInventoryDefaultsPropagationContext& InContext)
		: Context(InContext)
	{
		// Not persistent, loading, saving, etc.
		ArIsObjectReferenceCollector = true;
		SetShouldSkipCompilingAssets(true);
		ArShouldSkipBulkData = true;

		//SetWantBinaryPropertySerialization(true);
		//SetIsSaving(true);
	}

	virtual FArchive& operator<<(UObject*& Object) override
	{
		// Recursively serialize objects to reach all available nodes.
		if (Object && !Context.VisitedObjects.Contains(Object))
		{
			const TGuardValue ObjectGuard(Context.CurrentObject, Object);
			Context.VisitedObjects.Add(Object);
			Object->Serialize(*this);
		}

		return *this;
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("CommonInventoryDefaultsPropagation");
	}
};

// Used to avoid double serialization issue during defaults propagation.
static FCommonInventoryDefaultsPropagationArchive* CurrentPropagationArchive = nullptr;

FCommonInventoryDefaultsPropagator& FCommonInventoryDefaultsPropagator::Get()
{
	static FCommonInventoryDefaultsPropagator Instance;
	return Instance;
}

const FCommonInventoryDefaultsPropagator::FContext* FCommonInventoryDefaultsPropagator::GetContextFromArchive(FArchive& Ar) const
{
	if (IsPropagatingDefaults() && CurrentPropagationArchive == &Ar)
	{
		return &CurrentPropagationArchive->Context;
	}

	return nullptr;
}

void FCommonInventoryDefaultsPropagator::PropagateRegistryDefaults(FContext& InContext)
{
	checkf(IsInGameThread(), TEXT("FCommonInventoryDefaultsPropagator: Defaults propagation is only available on the game thread."));

	if (ensureMsgf(!IsPropagatingDefaults(), TEXT("FCommonInventoryDefaultsPropagator: Defaults propagation recurring is not allowed.")))
	{
		COMMON_INVENTORY_SCOPED_TRACE(FCommonInventoryDefaultsPropagator::PropagateRegistryDefaults);

		FlushAsyncLoading(); // Just to be safe with loading assets.

		if (TArray<UObject*> Objects; GatherObjectsForPropagation(InContext, Objects))
		{
			FCommonInventoryDefaultsPropagationArchive Ar(InContext);
			bIsPropagatingDefaults.store(true, std::memory_order::relaxed);
			const TGuardValue CurrentPropagationArchiveGuard(CurrentPropagationArchive, &Ar);
			ON_SCOPE_EXIT{ bIsPropagatingDefaults.store(false, std::memory_order::relaxed); };
			InContext.VisitedObjects.Reserve(InContext.VisitedObjects.Num() + Objects.Num());

			PreDefaultsPropagationDelegate.Broadcast(InContext);

			{
				COMMON_INVENTORY_SCOPED_TRACE(FCommonInventoryDefaultsPropagator::Propagation);

				for (UObject* Object : Objects)
				{
					Ar << Object;
				}
			}

			PostDefaultsPropagationDelegate.Broadcast(InContext);
		}
	}
}

bool FCommonInventoryDefaultsPropagator::GatherObjectsForPropagation(const FContext& InContext, TArray<UObject*>& OutObjects)
{
	COMMON_INVENTORY_SCOPED_TRACE(FCommonInventoryDefaultsPropagator::GatherObjectsForPropagation);

	OutObjects.Reserve(256); // Reserve some space to minimize initial reallocations.

	if (bool bSkipGathering = false; (GatherObjectsOverrideDelegate.ExecuteIfBound(InContext, OutObjects, bSkipGathering), !bSkipGathering))
	{
		if (ensureMsgf(!IAssetRegistry::GetChecked().IsLoadingAssets(), TEXT("FCommonInventoryDefaultsPropagator: Unable to gather all relevant objects during asset registry loading.")))
		{
			checkf(GEngine, TEXT("FCommonInventoryDefaultsPropagator: GEngine should be valid during objects gathering."));

			// Serialize active worlds.
			for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
			{
				if (UWorld* const World = WorldContext.World())
				{
					if (WorldContext.WorldType == EWorldType::Game || WorldContext.WorldType == EWorldType::PIE)
					{
						OutObjects.Emplace(World);
						OutObjects.Emplace(World->GetGameInstance());
					}
#if WITH_EDITOR
					// Don't modify the editor world during PIE.
					if (!GIsPlayInEditorWorld && WorldContext.WorldType == EWorldType::Editor)
					{
						OutObjects.Emplace(World);
					}
#endif
				}
			}

			// Don't modify the original assets during PIE.
			if (!GIsPlayInEditorWorld)
			{
				TArray<FAssetData> Referencers;

				for (const FCommonInventoryRegistryRecord& OriginalRecord : InContext.OriginalRegistryState.GetRecords())
				{
					CommonInventory::GetReferencers(OriginalRecord.GetPrimaryAssetId(), Referencers, /* bRecursively */ true);

					for (const FAssetData& AssetData : Referencers)
					{
						if (AssetData.IsAssetLoaded())
						{
							GetObjectsWithOuter(
								AssetData.GetPackage(),
								OutObjects,
								/* bIncludeNestedObjects */ true,
								/* ExclusionFlags */ RF_NoFlags /* RF_Transactional */,
								/* ExclusionInternalFlags */ EInternalObjectFlags::Garbage | EInternalObjectFlags::Async
							);
						}
					}

					Referencers.Reset();
				}
			}
		}
	}

	return !OutObjects.IsEmpty();
}
