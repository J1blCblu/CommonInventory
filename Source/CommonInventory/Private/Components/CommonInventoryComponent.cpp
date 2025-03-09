// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "Components/CommonInventoryComponent.h"

#include "Commands/CommonInventoryCommandController.h"
#include "CommonInventoryLog.h"
#include "CommonInventoryReplication.h"

#include "Net/UnrealNetwork.h"
#include "Net/Subsystems/NetworkSubsystem.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryComponent)

UCommonInventoryComponent::UCommonInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

UCommonInventoryComponent::~UCommonInventoryComponent() = default;

void UCommonInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	ensureMsgf(GetOwner()->IsUsingRegisteredSubObjectList(), TEXT("UCommonInventoryComponent requires the owner to set bReplicateUsingRegisteredSubObjectList."));

	if (GetNetMode() > ENetMode::NM_Standalone)
	{
		SetIsReplicated(true);
	}

	InventoryState.Initialize(DefaultCapacity);
}

void UCommonInventoryComponent::UninitializeComponent()
{
	InventoryState.Deinitialize();
	Super::UninitializeComponent();
}

void UCommonInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCommonInventoryComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UCommonInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.Condition = COND_None;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InventoryState, Params);
}

ELifetimeCondition UCommonInventoryComponent::GetReplicationCondition() const
{
	switch (ReplicationMode)
	{
	case ECommonInventoryReplicationMode::Never:		return COND_Never;
	case ECommonInventoryReplicationMode::Public:		return COND_SkipOwner;
	case ECommonInventoryReplicationMode::Protected:	return COND_NetGroup;
	case ECommonInventoryReplicationMode::Private:		return COND_Never;
	case ECommonInventoryReplicationMode::Auto:			return COND_NetGroup;
	default: checkNoEntry();							return COND_Never;
	}
}

bool UCommonInventoryComponent::GetComponentClassCanReplicate() const
{
	return GetReplicationCondition() != COND_Never;
}

FCommonInventoryView UCommonInventoryComponent::MakeInventoryView(const FCommonInventoryTraversingParams& TraversingParams) const
{
	return FCommonInventoryView();
}

void UCommonInventoryComponent::RegisterInventoryListener(const APlayerController* InPlayerController)
{
	if (ReplicationMode == ECommonInventoryReplicationMode::Protected || ReplicationMode == ECommonInventoryReplicationMode::Auto)
	{
		if (ensureMsgf(GetOwnerRole() == ROLE_Authority, TEXT("Unauthorized listener registration.")))
		{
			if (const UCommonInventoryReplication* const InventoryReplication = GetWorld()->GetSubsystem<UCommonInventoryReplication>())
			{
				const FName NetGroup = InventoryReplication->GetInventoryNetGroupForPlayer(InPlayerController);

				if (UNetworkSubsystem* const NetworkSubsystem = GetWorld()->GetSubsystem<UNetworkSubsystem>())
				{
					NetworkSubsystem->GetNetConditionGroupManager().RegisterSubObjectInGroup(this, NetGroup);
				}
			}
		}
	}
}

void UCommonInventoryComponent::UnregisterInventoryListener(const APlayerController* InPlayerController)
{
	if (ReplicationMode == ECommonInventoryReplicationMode::Protected || ReplicationMode == ECommonInventoryReplicationMode::Auto)
	{
		if (ensureMsgf(GetOwnerRole() == ROLE_Authority, TEXT("Unauthorized listener registration.")))
		{
			if (const UCommonInventoryReplication* const InventoryReplication = GetWorld()->GetSubsystem<UCommonInventoryReplication>())
			{
				const FName NetGroup = InventoryReplication->GetInventoryNetGroupForPlayer(InPlayerController);

				if (UNetworkSubsystem* const NetworkSubsystem = GetWorld()->GetSubsystem<UNetworkSubsystem>())
				{
					NetworkSubsystem->GetNetConditionGroupManager().UnregisterSubObjectFromGroup(this, NetGroup);
				}
			}
		}
	}
}

FCommonInventoryCommandController* UCommonInventoryComponent::GetCommandController() const
{
	if (CommandController == nullptr && GetOwnerRole() > ROLE_SimulatedProxy)
	{
		CommandController = MakeUnique<FCommonInventoryCommandController>(GetOwner());
	}

	return CommandController.Get();
}
