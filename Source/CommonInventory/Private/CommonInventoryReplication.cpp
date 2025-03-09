// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "CommonInventoryReplication.h"

#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryReplication)

bool UCommonInventoryReplication::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* const World = static_cast<UWorld*>(Outer))
	{
		return (World->IsNetMode(NM_ListenServer) || World->IsNetMode(NM_DedicatedServer)) && Super::ShouldCreateSubsystem(Outer);
	}

	return false;
}

bool UCommonInventoryReplication::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UCommonInventoryReplication::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Automatically assign a unique listen group for each player based on PlayerId.
	FGameModeEvents::OnGameModePostLoginEvent().AddUObject(this, &ThisClass::OnPostLogin);
	FGameModeEvents::OnGameModeLogoutEvent().AddUObject(this, &ThisClass::OnLogout);
}

void UCommonInventoryReplication::Deinitialize()
{
	Super::Deinitialize();

	FGameModeEvents::OnGameModePostLoginEvent().RemoveAll(this);
	FGameModeEvents::OnGameModeLogoutEvent().RemoveAll(this);
}

FName UCommonInventoryReplication::GetInventoryNetGroupForPlayer(const APlayerController* InPlayerController) const
{
	return NetGroups.FindRef(InPlayerController, NAME_None);
}

void UCommonInventoryReplication::OnPostLogin(AGameModeBase*, APlayerController* InPlayerController)
{
	if (InPlayerController && InPlayerController->PlayerState)
	{
		InPlayerController->IncludeInNetConditionGroup(NetGroups.Emplace(InPlayerController, FName(PlayerNetGroup, InPlayerController->PlayerState->GetPlayerId())));
	}
}

void UCommonInventoryReplication::OnLogout(AGameModeBase*, AController* InController)
{
	if (APlayerController* const PlayerController = Cast<APlayerController>(InController))
	{
		PlayerController->RemoveFromNetConditionGroup(NetGroups.FindAndRemoveChecked(PlayerController));
	}
}
