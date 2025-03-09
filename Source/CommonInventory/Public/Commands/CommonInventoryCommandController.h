// Copyright 2025 Ivan Baktenkov. All Rights Reserved.

#pragma once

#include "Commands/CommonInventoryCommand.h"

#include "CommonInventoryCommandController.generated.h"

/**
 * 
 */
USTRUCT()
struct FCommonInventoryCommandBunch
{
	GENERATED_BODY()

	/**  */
	UPROPERTY()
	TSubclassOf<UCommonInventoryCommand> CommandClass;
	
	/**  */
	UPROPERTY()
	FVariadicStruct CommandPayload;

	/**  */
	UPROPERTY()
	float Timestamp = 0.f;

	/**  */
	UPROPERTY(NotReplicated)
	uint32 CmdSeq = 0;

	/**  */
	UPROPERTY(NotReplicated)
	uint32 CmdFrame = 0;
};

// Problems that GAS's prediction implementation is trying to solve:
// "Can I do this?" Basic protocol for prediction.
// "Undo" How to undo side effects when a prediction fails.
// "Redo" How to avoid replaying side effects that we predicted locally but that also get replicated from the server.
// "Completeness" How to be sure we /really/ predicted all side effects.
// "Dependencies" How to manage dependent prediction and chains of predicted events.
// "Override" How to override state predictively that is otherwise replicated/owned by the server.

// https://vorixo.github.io/devtricks/data-stream/
// https://www.youtube.com/watch?v=qOtLJq8ZHUs&t=1265s
// https://developer.valvesoftware.com/wiki/Prediction
// https://www.gamedev.net/forums/topic/703386-understanding-lag-compensation-timestamps/
// https://www.reddit.com/r/gamedev/comments/8zo660/which_lag_compensation_technique_should_i_use/
// https://www.reddit.com/r/Unity3D/comments/zfsaqp/what_better_way_could_i_use_to_implement_lag/
// https://developer.valvesoftware.com/wiki/Latency_Compensating_Methods_in_Client/Server_In-game_Protocol_Design_and_Optimization#Lag_Compensation
// https://vercidium.com/blog/lag-compensation/

/**
 * 
 */
struct COMMONINVENTORY_API FCommonInventoryCommandController
{
	FCommonInventoryCommandController(const AActor* InAuthorizedOwner);

	/**  */
	template<typename T, typename... Args>
	ECommonInventoryCommandExecutionResult ExecuteCommand(Args... InArgs)
	{
		static_assert(std::derived_from<T, UCommonInventoryCommand>);

		return ECommonInventoryCommandExecutionResult::Failure;
	}

private:

	/** Raw pointer to the authorized owner. */
	const AActor* AuthOwner = nullptr;
};
