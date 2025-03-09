// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "VariadicStruct.h"

#include "CommonInventoryCommand.generated.h"

class AActor;
class FArchive;
class UCommonInventoryComponent;

/** Command execution polices. */
enum class ECommonInventoryCommandExecutionPolicy
{
	Authoritative,	// Only the server can initiate a command.
	Predictive,		// Only the server and local clients can initiate a command.
};

/**  */
enum class ECommonInventoryCommandExecutionResult
{
	Success,	// 
	Failure,	// 
};

/**
 * 
 */
USTRUCT(meta = (Hidden))
struct FCommonInventoryCommandExecutionContext
{
	GENERATED_BODY()

	/**  */
	UPROPERTY()
	TObjectPtr<AActor> AuthOwner;

	/**  */
	UPROPERTY()
	TObjectPtr<UCommonInventoryComponent> SourceComponent = nullptr;

	/**  */
	UPROPERTY()
	TObjectPtr<UCommonInventoryComponent> TargetComponent = nullptr;

	/**  */
	UPROPERTY()
	FVariadicStruct CommandPayload;
};

/**
 * Stateless command processor.
 */
UCLASS(Abstract, Hidden)
class COMMONINVENTORY_API UCommonInventoryCommand : public UObject
{
	GENERATED_BODY()

	UCommonInventoryCommand();

public: // Interface

	/**  */
	virtual void InitializePayload(FVariadicStruct& OutPayload) const;

	/**  */
	virtual ECommonInventoryCommandExecutionResult CanExecute(FCommonInventoryCommandExecutionContext& ExecutionContext) const;

	/**  */
	virtual ECommonInventoryCommandExecutionResult Execute(FCommonInventoryCommandExecutionContext& ExecutionContext) const;

	/**  */
	virtual ECommonInventoryCommandExecutionResult Rollback(FCommonInventoryCommandExecutionContext& ExecutionContext) const;
	// @TODO: Theoretically we can automatically track changes and reapply them instead of requiring to implement Rollback().

protected:

	/**  */
	ECommonInventoryCommandExecutionPolicy ExecutionPolicy;
};
