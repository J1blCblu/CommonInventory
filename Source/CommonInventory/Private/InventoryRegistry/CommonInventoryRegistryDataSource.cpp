// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "InventoryRegistry/CommonInventoryRegistryDataSource.h"

#include "Misc/AssertionMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryRegistryDataSource)

void UCommonInventoryRegistryDataSource::Initialize(ICommonInventoryRegistryBridge* InRegistryBridge)
{
	checkf(InRegistryBridge, TEXT("UCommonInventoryRegistryDataSource: Failed to initialize RegistryBridge."));
	RegistryBridge = InRegistryBridge;
}

void UCommonInventoryRegistryDataSource::PostInitialize()
{
	bIsInitialized = true;
}

void UCommonInventoryRegistryDataSource::Deinitialize()
{
	RegistryBridge = nullptr;
	bIsInitialized = false;
}

void UCommonInventoryRegistryDataSource::ForceRefresh(bool bSynchronous /* = false */)
{

}

void UCommonInventoryRegistryDataSource::FlushPendingRefresh()
{

}

void UCommonInventoryRegistryDataSource::CancelPendingRefresh()
{

}

bool UCommonInventoryRegistryDataSource::IsPendingRefresh() const
{
	return false;
}

bool UCommonInventoryRegistryDataSource::IsRefreshing() const
{
	return false;
}

#if WITH_EDITOR

void UCommonInventoryRegistryDataSource::OnCookStarted()
{
	
}

void UCommonInventoryRegistryDataSource::OnCookFinished()
{
	
}

bool UCommonInventoryRegistryDataSource::VerifyAssumptionsForCook(const ITargetPlatform* InTargetPlatform) const
{
	return true;
}

bool UCommonInventoryRegistryDataSource::RequestPIEPermission(FString& OutReason, bool bIsSimulating) const
{
	return IsInitialized();
}

#endif // WITH_EDITOR
