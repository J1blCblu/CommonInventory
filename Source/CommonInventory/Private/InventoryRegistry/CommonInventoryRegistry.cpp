// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "InventoryRegistry/CommonInventoryRegistry.h"

#include "CommonInventoryLog.h"
#include "CommonInventorySettings.h"
#include "CommonInventoryTrace.h"

#include "Async/UniqueLock.h"
#include "CoreGlobals.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProperties.h"
#include "Misc/Commandline.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CoreMisc.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Net/RepLayout.h"
#include "Templates/UnrealTemplate.h"

#include <atomic>

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryRegistry)

// Internal instance of the registry.
static std::atomic<UCommonInventoryRegistry*> RegistryInstance = nullptr;

// Delegate to broadcast on initialization complete.
static FSimpleMulticastDelegate OnInventoryRegistryInitializedDelegate;

// Kind of RWLock where the game thread requires a lock only for writes.
static UE::TDynamicUniqueLock<FCriticalSection> AcquireCriticalSectionForRead(FCriticalSection& InCriticalSection)
{
	return IsInGameThread() ? UE::TDynamicUniqueLock(InCriticalSection, UE::DeferLock) : UE::TDynamicUniqueLock(InCriticalSection);
}

/************************************************************************/
/* Inventory Registry                                                   */
/************************************************************************/

bool UCommonInventoryRegistry::IsInitialized()
{
	return RegistryInstance.load(std::memory_order::relaxed) != nullptr;
}

UCommonInventoryRegistry& UCommonInventoryRegistry::Get()
{
	UCommonInventoryRegistry* const Ptr = GetPtr();
	checkf(Ptr, TEXT("InventoryRegistry is not yet initialized."));
	return *Ptr;
}

UCommonInventoryRegistry* UCommonInventoryRegistry::GetPtr()
{
	return RegistryInstance.load(std::memory_order::acquire); // consume?
}

void UCommonInventoryRegistry::CallOrRegister_OnInventoryRegistryInitialized(FSimpleMulticastDelegate::FDelegate&& Delegate)
{
	if (UCommonInventoryRegistry::IsInitialized())
	{
		Delegate.Execute();
	}
	else
	{
		OnInventoryRegistryInitializedDelegate.Add(MoveTemp(Delegate));
	}
}

FString UCommonInventoryRegistry::GetRegistryFilename()
{
	return FPaths::Combine(FPaths::ProjectDir(), TEXT("InventoryRegistry.bin"));
}

FString UCommonInventoryRegistry::GetDevelopmentRegistryFilename()
{
	return FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("DevelopmentInventoryRegistry.bin"));
}

bool UCommonInventoryRegistry::ShouldCreateSubsystem(UObject* Outer) const
{
	// It's unsafe to startup without asset scanning.
	return !FParse::Param(FCommandLine::Get(), TEXT("SkipAssetScan"));
}

void UCommonInventoryRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	COMMON_INVENTORY_SCOPED_TRACE(UCommonInventoryRegistry::Initialize);

	if (UClass* const DataSourceClass = UCommonInventorySettings::Get()->DataSourceClassName.ResolveClass())
	{
		// Cache traits from CDO.
		DataSourceTraits = DataSourceClass->GetDefaultObject<UCommonInventoryRegistryDataSource>()->GetTraits();

		// Try to load the registry state.
		if ((bWasLoaded = TryLoadFromFile()) == true)
		{
			ConditionallyUpdateNetworkChecksum();
		}

		if ((DataSource = NewObject<UCommonInventoryRegistryDataSource>(this, DataSourceClass, FName("InventoryRegistryDataSource"), RF_Transient)))
		{
			DataSource->Initialize(this);
		}
	}

	checkf(DataSource, TEXT("UInventoryRegistry: Failed to create DataSource of type: '%s'."), *UCommonInventorySettings::Get()->DataSourceClassName.ToString());

	// Give the data source some time to complete initialization.
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &ThisClass::PostInitialize);
}

void UCommonInventoryRegistry::PostInitialize()
{
	COMMON_INVENTORY_SCOPED_TRACE(UCommonInventoryRegistry::PostInitialize);
	
	// Initialize redirects.
	FCommonInventoryRedirects::Get();

	// Complete the initialization of persistent data sources.
	DataSource->PostInitialize();

	RegistryInstance.store(this, std::memory_order::release);
	OnInventoryRegistryInitializedDelegate.Broadcast();
	OnInventoryRegistryInitializedDelegate.Clear();
}

void UCommonInventoryRegistry::Deinitialize()
{
	Super::Deinitialize();
	DataSource->Deinitialize();
	RegistryInstance.store(nullptr, std::memory_order::relaxed);

#if WITH_EDITOR
	if (GIsEditor && !IsRunningCommandlet())
	{
		if (DataSourceTraits.bSupportsDevelopmentCooking)
		{
			RegistryState.SaveToFile(GetDevelopmentRegistryFilename(), /* bIsCooking */ false);
		}
		else
		{
			// Delete the registry cache, to avoid loading extremely outdated data in the future.
			IFileManager::Get().Delete(*GetDevelopmentRegistryFilename(), /* RequireExists */ false, /* EvenReadOnly */ true, /* Quiet */ false);
		}
	}
#endif
}

bool UCommonInventoryRegistry::TryLoadFromFile()
{
	COMMON_INVENTORY_SCOPED_TRACE(UCommonInventoryRegistry::TryLoadFromFile);

#if WITH_EDITOR
	if (DataSourceTraits.bSupportsDevelopmentCooking && GIsEditor && !IsRunningCommandlet())
	{
		return RegistryState.LoadFromFile(GetDevelopmentRegistryFilename(), /* bIsCooked */ false);
	}
#else
	// Try to load the registry state from disk if requested.
	if (DataSourceTraits.bSupportsCooking && FPlatformProperties::RequiresCookedData() && (IsRunningGame() || IsRunningDedicatedServer()))
	{
		return RegistryState.LoadFromFile(GetRegistryFilename(), /* bIsCooked */ true);
	}
#endif

	return false;
}

void UCommonInventoryRegistry::ForceRefresh(bool bSynchronous)
{
	if (DataSource)
	{
		DataSource->ForceRefresh(bSynchronous);
	}
}

void UCommonInventoryRegistry::FlushPendingRefresh()
{
	if (DataSource)
	{
		DataSource->FlushPendingRefresh();
	}
}

void UCommonInventoryRegistry::CancelPendingRefresh()
{
	if (DataSource)
	{
		DataSource->CancelPendingRefresh();
	}
}

bool UCommonInventoryRegistry::IsPendingRefresh() const
{
	return !DataSource || DataSource->IsPendingRefresh();
}

bool UCommonInventoryRegistry::IsRefreshing() const
{
	return DataSource && DataSource->IsRefreshing();
}

bool UCommonInventoryRegistry::ResetItem(FPrimaryAssetId InPrimaryAssetId, FVariadicStruct& InPayload) const
{
	UE::TDynamicUniqueLock ScopeLock{ AcquireCriticalSectionForRead(CriticalSection) };

	if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRegistryRecord(InPrimaryAssetId))
	{
		InPayload.InitializeAs(RegistryRecord->DefaultPayload.GetScriptStruct(), RegistryRecord->DefaultPayload.GetMemory());
		return true;
	}

	return false;
}

bool UCommonInventoryRegistry::ValidateItem(FPrimaryAssetId InPrimaryAssetId, const FVariadicStruct& InPayload) const
{
	UE::TDynamicUniqueLock ScopeLock{ AcquireCriticalSectionForRead(CriticalSection) };

	if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRegistryRecord(InPrimaryAssetId))
	{
		return InPayload.GetScriptStruct() == RegistryRecord->DefaultPayload.GetScriptStruct();
	}

	return !InPrimaryAssetId.IsValid() && !InPayload.IsValid();
}

bool UCommonInventoryRegistry::SynchronizeItem(FPrimaryAssetId& InPrimaryAssetId, FVariadicStruct& InPayload) const
{
	UE::TDynamicUniqueLock ScopeLock{ AcquireCriticalSectionForRead(CriticalSection) };

	// Try to redirect if possible.
	if (InPrimaryAssetId.IsValid() && !ContainsRecord(InPrimaryAssetId))
	{
		FCommonInventoryRedirects::Get().TryRedirect(InPrimaryAssetId);
	}

	if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRegistryRecord(InPrimaryAssetId))
	{
		if (RegistryRecord->DefaultPayload.GetScriptStruct() != InPayload.GetScriptStruct())
		{
			InPayload.InitializeAs(RegistryRecord->DefaultPayload.GetScriptStruct(), RegistryRecord->DefaultPayload.GetMemory());
		}

		return true;
	}

	return false;
}

bool UCommonInventoryRegistry::SerializeItem(FArchive& Ar, FPrimaryAssetId& InPrimaryAssetId, FVariadicStruct& InPayload) const
{
#if WITH_EDITOR
	checkf(!DataSourceTraits.bSupportsCooking || !Ar.IsCooking() || bIsCooking, TEXT("InventoryRegistry must be in the same cooking state as FArchive."));
#endif

	// Check if we're propagating defaults and received the relevant archive.
	if (const FCommonInventoryDefaultsPropagationContext* const PropagationContext = FCommonInventoryDefaultsPropagator::Get().GetContextFromArchive(Ar))
	{
		PropagateItemDefaults(*PropagationContext, InPrimaryAssetId, InPayload);
	}
	else
	{
		UE::TDynamicUniqueLock ScopeLock{ AcquireCriticalSectionForRead(CriticalSection) };

		// Just in case the defaults propagation was unable to reach the item.
		if (Ar.IsSaving() && InPrimaryAssetId.IsValid() && !ContainsRecord(InPrimaryAssetId))
		{
			if (!FCommonInventoryRedirects::Get().TryRedirect(InPrimaryAssetId))
			{
				InPrimaryAssetId = FPrimaryAssetId();
			}
		}

		Ar << InPrimaryAssetId;

		// Editor and client-side saved data might be outdated.
		if (Ar.IsLoading() && Ar.IsPersistent() && InPrimaryAssetId.IsValid())
		{
			if (Ar.IsLoadingFromCookedPackage() && DataSourceTraits.bIsPersistent)
			{
				// We assume that the data was properly synchronized during the cook.
				checkf(!FCommonInventoryRedirects::Get().IsStale(InPrimaryAssetId), TEXT("Failed to validate FCommonItem during loading. Make sure to re-cook relevant packages."));
			}
			else if (!(Ar.GetPortFlags() & (PPF_DuplicateForPIE | PPF_Duplicate)))
			{
				FCommonInventoryRedirects::Get().TryRedirect(InPrimaryAssetId);
			}
		}

		FConstStructView Defaults;

		if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRegistryRecord(InPrimaryAssetId))
		{
			Defaults = RegistryRecord->DefaultPayload;
		}
		else
		{
			InPrimaryAssetId = FPrimaryAssetId();
		}

		InPayload.Serialize(Ar, &Defaults);
	}

	return true;
}

void UCommonInventoryRegistry::PropagateItemDefaults(const FCommonInventoryDefaultsPropagator::FContext& InContext, FPrimaryAssetId& InPrimaryAssetId, FVariadicStruct& InPayload) const
{
	check(IsInGameThread());

	// Check if we should actually propagate defaults.
	if (InPrimaryAssetId.IsValid() && InContext.OriginalRegistryState.ContainsRecord(InPrimaryAssetId))
	{
		// Just in case the item was renamed.
		const FPrimaryAssetId OriginalPrimaryAssetId = InPrimaryAssetId;
		FCommonInventoryRedirects::Get().TryRedirect(InPrimaryAssetId);

		// Check if it's still in the registry, otherwise it has been removed.
		if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRegistryRecord(InPrimaryAssetId))
		{
			// Mark the package as dirty if payload uses native serialization.
			// Otherwise it will restore the original defaults on the next load.
			[[maybe_unused]] bool bMarkPackageDirty = false;

			// Reinitialize the payload if differs from the default.
			if (RegistryRecord->DefaultPayload.GetScriptStruct() != InPayload.GetScriptStruct())
			{
				InPayload.InitializeAs(RegistryRecord->DefaultPayload.GetScriptStruct(), RegistryRecord->DefaultPayload.GetMemory());
			}
			else
			{
				const FCommonInventoryRegistryRecord& OriginalRecord = InContext.OriginalRegistryState.GetRecord(OriginalPrimaryAssetId);

				// Enumerate top-level properties and propagate changes.
				for (TPropertyValueIterator<FProperty> It(InPayload.GetScriptStruct(), InPayload.GetMemory()); It; ++It)
				{
					const uint8* const DefaultValue = RegistryRecord->DefaultPayload.GetMemory() + It.Key()->GetOffset_ForInternal();
					const uint8* const OriginalValue = OriginalRecord.DefaultPayload.GetMemory() + It.Key()->GetOffset_ForInternal();

					// Skip unchanged properties.
					if (It.Key()->Identical(DefaultValue, OriginalValue, PPF_ForDiff))
					{
						continue;
					}

					// Update the value if the property matches the previous default value.
					if (It.Key()->Identical(It.Value(), OriginalValue, PPF_ForDiff))
					{
						It.Key()->CopyCompleteValue(const_cast<void*>(It.Value()), DefaultValue);
						bMarkPackageDirty = true;
					}
				}
			}

#if WITH_EDITOR
			if (bMarkPackageDirty && InPayload.GetScriptStruct()->StructFlags & STRUCT_SerializeNative)
			{
				// Don't mark the object as dirty if it is the initial fixup.
				if (InContext.CurrentObject && !InContext.bIsInitialFixup)
				{
					InContext.CurrentObject->MarkPackageDirty();
				}
			}
#endif // WITH_EDITOR
		}
		else
		{
			InPrimaryAssetId = FPrimaryAssetId();
			InPayload.Reset();
		}
	}
}

FCommonInventoryRegistryNetSerializationContext UCommonInventoryRegistry::NetSerializeItem(FArchive& Ar, FPrimaryAssetId& InPrimaryAssetId, bool& bOutSuccess) const
{
	FCommonInventoryRegistryNetSerializationContext OutContext{ InPrimaryAssetId, bOutSuccess };
	uint32 RepIndex = CommonInventory::INVALID_REPLICATION_INDEX;
	[[maybe_unused]] uint32 Checksum = 0;

	if (Ar.IsSaving())
	{
		if ((OutContext.RegistryRecord = RegistryState.GetRecordPtr(InPrimaryAssetId)) != nullptr)
		{
			RepIndex = OutContext.RegistryRecord->RepIndex;

#if COMMON_INVENTORY_WITH_NETWORK_CHECKSUM
			Checksum = OutContext.RegistryRecord->GetChecksum();
#endif
		}
	}

	Ar.SerializeBits(&RepIndex, RegistryState.GetRepIndexEncodingBitsNum());

#if COMMON_INVENTORY_WITH_NETWORK_CHECKSUM
	Ar << Checksum;
#endif

	if (Ar.IsLoading())
	{
		[[maybe_unused]] uint32 LocalChecksum = 0;

		if ((OutContext.RegistryRecord = RegistryState.GetRecordFromReplication(RepIndex)) != nullptr)
		{
			InPrimaryAssetId = OutContext.RegistryRecord->GetPrimaryAssetId();

#if COMMON_INVENTORY_WITH_NETWORK_CHECKSUM
			LocalChecksum = OutContext.RegistryRecord->GetChecksum();
#endif
		}
		else
		{
			InPrimaryAssetId = FPrimaryAssetId();

			// This is not a critical error yet until we try to serialize the payload.
			if (!ensureMsgf(RepIndex == CommonInventory::INVALID_REPLICATION_INDEX, TEXT("Failed to match RepIndex %u with any FPrimaryAssetId."), RepIndex))
			{
				OutContext.bOutSuccess = false;
			}
		}

#if COMMON_INVENTORY_WITH_NETWORK_CHECKSUM
		// This is not a critical error yet until we try to serialize the payload.
		if (!ensureMsgf(Checksum == LocalChecksum, TEXT("Network checksum mismatch encountered for '%s': Local(%#x), Remote(%#x)."), *InPrimaryAssetId.ToString(), LocalChecksum, Checksum))
		{
			OutContext.bOutSuccess = false;
		}
#endif
	}

	return OutContext;
}

void UCommonInventoryRegistry::NetSerializeItemPayload(FArchive& Ar, FVariadicStruct& InPayload, FCommonInventoryRegistryNetSerializationContext& InContext) const
{
	bool bHasPayload = InContext.RegistryRecord && InContext.RegistryRecord->DefaultPayload.IsValid();
	Ar.SerializeBits(&bHasPayload, 1);

	if (bHasPayload)
	{
		// Writer have serialized the payload, but the reader have failed to find the template for deserialization.
		if (!ensureMsgf(InContext.RegistryRecord, TEXT("Desync encountered during net-serialization.")))
		{
			InContext.bOutSuccess = false;
			InPayload.Reset();
			Ar.SetError();
			return;
		}

		// Synchronize the type without copying the defaults.
		if (InPayload.GetScriptStruct() != InContext.RegistryRecord->DefaultPayload.GetScriptStruct())
		{
			InPayload.InitializeAs(InContext.RegistryRecord->DefaultPayload.GetScriptStruct(), /* InStructMemory */ nullptr);
		}

		// Net serialize the actual value.
		if (const UScriptStruct* const ScriptStruct = InPayload.GetScriptStruct())
		{
			uint8* const Memory = InPayload.GetMutableMemory();

			// Use native serialization if possible.
			if (ScriptStruct->StructFlags & STRUCT_NetSerializeNative)
			{
				ScriptStruct->GetCppStructOps()->NetSerialize(Ar, /* Map */ nullptr, InContext.bOutSuccess, Memory);
			}
			else
			{
				check(GEngine);

				// UPackageMap is nullptr because of net shared serialization.
				if (UWorld* const CurrentWorld = GEngine->GetCurrentPlayWorld())
				{
					bool bHasUnmapped = false;
					UScriptStruct* const MutableScriptStruct = const_cast<UScriptStruct*>(ScriptStruct);
					const TSharedRef<FRepLayout> RepLayout = CurrentWorld->GetNetDriver()->GetStructRepLayout(MutableScriptStruct).ToSharedRef();
					RepLayout->SerializePropertiesForStruct(MutableScriptStruct, static_cast<FBitArchive&>(Ar), nullptr, Memory, bHasUnmapped);
				}
				else
				{
					InContext.bOutSuccess = false;
					InPayload.Reset();
					Ar.SetError();
					return;
				}
			}
		}
	}
	else
	{
		InPayload.Reset();
	}
}

void UCommonInventoryRegistry::CheckDataSourceContractViolation() const
{
	checkf(IsInGameThread(), TEXT("UInventoryRegistry: Data source attempts to modify the registry outside the of the game thread."));
	checkf(!DataSourceTraits.bIsPersistent || !IsInitialized() || !GIsPlayInEditorWorld, TEXT("UInventoryRegistry: Persistent data source attempts to modify the registry."));

#if WITH_EDITOR
	checkf(!IsCooking(), TEXT("UInventoryRegistry: Data source attempts to modify the cooked registry state."));
#endif
}

int32 UCommonInventoryRegistry::AppendRecords(TConstArrayView<FCommonInventoryRegistryRecord> InRecords)
{
	CheckDataSourceContractViolation();
	
	FCommonInventoryDefaultsPropagationContext PropagationContext;
	
	// Copy existing data for further propagation if needed.
	for (const FCommonInventoryRegistryRecord& Record : InRecords)
	{
		if (const FCommonInventoryRegistryRecord* const OriginalRecord = GetRegistryRecord(Record.GetPrimaryAssetId()))
		{
			PropagationContext.OriginalRegistryState.AppendData(*OriginalRecord);
		}
	}

	const int32 NumAdded = InRecords.Num() - PropagationContext.OriginalRegistryState.GetRecordsNum();

	if (!InRecords.IsEmpty())
	{
		{
			FScopeLock Lock(&CriticalSection);

			for (const FCommonInventoryRegistryRecord& Record : InRecords)
			{
				RegistryState.AppendData(Record);
			}
		}

		OnPostRefresh(PropagationContext);
	}

	return NumAdded;
}

int32 UCommonInventoryRegistry::RemoveRecords(TConstArrayView<FPrimaryAssetId> InRecordIds)
{
	CheckDataSourceContractViolation();

	FCommonInventoryDefaultsPropagationContext PropagationContext;

	// Copy existing data for further propagation.
	for (const FPrimaryAssetId RecordId : InRecordIds)
	{
		if (const FCommonInventoryRegistryRecord* const RegistryRecord = GetRegistryRecord(RecordId))
		{
			PropagationContext.OriginalRegistryState.AppendData(*RegistryRecord);
		}
	}

	int32 NumRemoves = 0;

	if (PropagationContext.OriginalRegistryState.HasRecords())
	{
		{
			FScopeLock Lock(&CriticalSection);

			for (const FPrimaryAssetId RecordId : InRecordIds)
			{
				// Remove the records.
				if (RegistryState.RemoveData(RecordId))
				{
					++NumRemoves;
				}
			}
		}

		OnPostRefresh(PropagationContext);
	}

	return NumRemoves;
}

void UCommonInventoryRegistry::ResetRecords(TConstArrayView<FCommonInventoryRegistryRecord> InRecords)
{
	CheckDataSourceContractViolation();

	FCommonInventoryDefaultsPropagationContext PropagationContext;

	if (RegistryState.HasRecords())
	{
		PropagationContext.OriginalRegistryState = MoveTemp(RegistryState);
		PropagationContext.bIsInitialFixup = !IsInitialized();
		PropagationContext.bWasReset = true;
	}

	{
		FScopeLock Lock(&CriticalSection);
		RegistryState.Reset(InRecords);
	}

	OnPostRefresh(PropagationContext);
}

void UCommonInventoryRegistry::OnPostRefresh(FCommonInventoryDefaultsPropagationContext& InPropagationContext)
{
#if WITH_EDITOR
	// Report any issues.
	ReportInvariantViolation();
#endif

	// Try update the checksum before diffing so it can use a faster comparator.
	ConditionallyUpdateNetworkChecksum();

	// Leave only the actual changes.
	InPropagationContext.OriginalRegistryState.DiffRecords(RegistryState);

	// Propagate the changes across reflected types.
	if (InPropagationContext.OriginalRegistryState.HasRecords())
	{
		FCommonInventoryDefaultsPropagator::Get().PropagateRegistryDefaults(InPropagationContext);
	}

	// Finally, notify the listeners.
	PostRefreshDelegate.Broadcast();
}

void UCommonInventoryRegistry::ConditionallyUpdateNetworkChecksum()
{
	// Register a custom network version for validating network compatibility.
	// Network replays might require additional serialization of the registry state.
	if (DataSourceTraits.bIsPersistent && UCommonInventorySettings::Get()->bRegisterNetworkCustomVersion)
	{
		constexpr FGuid NetworkVersionGuid{ 0x06A2707A, 0xF79A93F9, 0x49E576A9, 0x9EA1B0FA };
		static const FName FriendlyName{ "CommonInventoryRegistryChecksum" };
		const uint32 Checksum = RegistryState.GetChecksum();

#if WITH_EDITOR
		// Suppress the warning about changing custom versions.
		const ELogVerbosity::Type OriginalVerbosity = LogNetVersion.GetVerbosity();
		ON_SCOPE_EXIT{ LogNetVersion.SetVerbosity(OriginalVerbosity); };
		LogNetVersion.SetVerbosity(ELogVerbosity::Error);
#endif

		FNetworkVersion::RegisterNetworkCustomVersion(NetworkVersionGuid, Checksum, Checksum, FriendlyName);
	}
}

#if WITH_EDITOR

void UCommonInventoryRegistry::OnCookStarted()
{
	if (!bIsCooking)
	{
		if (DataSourceTraits.bSupportsCooking)
		{
			DataSource->OnCookStarted();
		}

		bIsCooking = true;
	}
}

void UCommonInventoryRegistry::OnCookFinished()
{
	if (bIsCooking)
	{
		bIsCooking = false;

		if (DataSourceTraits.bSupportsCooking)
		{
			DataSource->OnCookFinished();
		}
	}
}

void UCommonInventoryRegistry::WriteForCook(const ITargetPlatform* InTargetPlatform, const FString& InFilename)
{
	check(IsCooking());

	if (DataSourceTraits.bSupportsCooking /* || FParse::Param(FCommandLine::Get(), TEXT("CookInventoryRegistry")) */)
	{
		COMMON_INVENTORY_SCOPED_TRACE(UCommonInventoryRegistry::WriteForCook);

		// It's much safer to generate the state at runtime if some assumptions were failed.
		if (DataSource->VerifyAssumptionsForCook(InTargetPlatform))
		{
			if (!RegistryState.SaveToFile(InFilename, /* bIsCooking */ true))
			{
				COMMON_INVENTORY_LOG(Warning, "InventoryRegistry: Failed to write the state into '%s' during the cook.", *InFilename);
			}
		}
	}
}

void UCommonInventoryRegistry::ReportInvariantViolation() const
{
	const TArrayView RecordsView = GetRegistryRecords();

	// Report stale name redirects.
	for (const auto [OldName, NewName] : FCommonInventoryRedirects::Get().GetNameRedirects())
	{
		if (!RecordsView.ContainsByPredicate([NewName](const FCommonInventoryRegistryRecord& InRecord) { return InRecord.GetPrimaryAssetName() == NewName; }))
		{
			COMMON_INVENTORY_LOG(Warning, "Inventory Registry contains a stale name redirect: '%s' -> '%s'.", *OldName.ToString(), *NewName.ToString());
		}
	}

	// Report stale type redirects.
	for (const auto [OldType, NewType] : FCommonInventoryRedirects::Get().GetTypeRedirects())
	{
		if (!RecordsView.ContainsByPredicate([NewType](const FCommonInventoryRegistryRecord& InRecord) { return InRecord.GetPrimaryAssetType() == NewType; }))
		{
			COMMON_INVENTORY_LOG(Warning, "Inventory Registry contains a stale type redirect: '%s' -> '%s'.", *OldType.ToString(), *NewType.ToString());
		}
	}

	TArray<const FStructProperty*> EncounteredStructProps;

	// Check for object references in DefaultPayload.
	for (const FCommonInventoryRegistryRecord& Record : GetRegistryRecords())
	{
		if (Record.DefaultPayload.IsValid())
		{
			for (TFieldIterator<FProperty> It{ Record.DefaultPayload.GetScriptStruct() }; It; ++It)
			{
				// Function is recursive and will handle nested types for us.
				if (It->ContainsObjectReference(EncounteredStructProps, EPropertyObjectReferenceType::Strong | EPropertyObjectReferenceType::Weak))
				{
					COMMON_INVENTORY_LOG(Error, "InventoryRegistry: Encountered DefaultPayload with object references in %s ('%s'), which is not allowed.",
										 *Record.GetPrimaryAssetId().ToString(), *Record.AssetPath.ToString());
					break;
				}
			}
		}
	}
}

#endif // WITH_EDITOR
