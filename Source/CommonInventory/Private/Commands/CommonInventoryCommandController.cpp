// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#include "Commands/CommonInventoryCommandController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonInventoryCommandController)

FCommonInventoryCommandController::FCommonInventoryCommandController(const AActor* InAuthorizedOwner)
	: AuthOwner(InAuthorizedOwner)
{
	check(AuthOwner);
}


