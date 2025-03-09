// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "Commands/CommonInventoryCommand.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryCommand)

UCommonInventoryCommand::UCommonInventoryCommand()
	: ExecutionPolicy(ECommonInventoryCommandExecutionPolicy::Authoritative)
{

}

void UCommonInventoryCommand::InitializePayload(FVariadicStruct& OutPayload) const
{

}

ECommonInventoryCommandExecutionResult UCommonInventoryCommand::CanExecute(FCommonInventoryCommandExecutionContext& ExecutionContext) const
{
	return ECommonInventoryCommandExecutionResult::Failure;
}

ECommonInventoryCommandExecutionResult UCommonInventoryCommand::Execute(FCommonInventoryCommandExecutionContext& ExecutionContext) const
{
	return ECommonInventoryCommandExecutionResult::Failure;
}

ECommonInventoryCommandExecutionResult UCommonInventoryCommand::Rollback(FCommonInventoryCommandExecutionContext& ExecutionContext) const
{
	return ECommonInventoryCommandExecutionResult::Failure;
}
