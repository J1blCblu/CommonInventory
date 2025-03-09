// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "CommonInventoryReplication.generated.h"

class APlayerController;
class AGameModeBase;
class AController;

/**
 * A server only subsystem for managing replication specifics.
 */
UCLASS(MinimalAPI, Hidden)
class UCommonInventoryReplication : public UWorldSubsystem
{
	GENERATED_BODY()

	UCommonInventoryReplication() = default;

	/** The default name template for the player NetGroup specialization. */
	static inline const FName PlayerNetGroup = FName(TEXT("CommonInventoryPlayerNetGroup"));

public:

	// Returns the NetGroup for the provided player.
	COMMONINVENTORY_API FName GetInventoryNetGroupForPlayer(const APlayerController* InPlayerController) const;

private:

	void OnPostLogin(AGameModeBase*, APlayerController* InPlayerController);
	void OnLogout(AGameModeBase*, AController* InController);

	// Overrides
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	TMap<const APlayerController*, FName, TInlineSetAllocator<8>> NetGroups;
};

/**
 * Excerpt from Satisfactory:
 * This all works very well at the moment, but there is always more we can do.
 * In the future we could work to prefetch inventory data, trying to predict potential targets before you interact with them and fetch them just in case,
 * minimizing or even removing latency. But that is work for another update and can cause serious data spikes if we are not careful.
 */
UCLASS(Abstract, Hidden)
class COMMONINVENTORY_API UCommonInventoryPrefetcher : public UObject
{
	GENERATED_BODY()
};
