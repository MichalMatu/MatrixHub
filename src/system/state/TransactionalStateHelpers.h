/**
 * @file TransactionalStateHelpers.h
 * @brief Shared helpers for transactional custom settings services
 */

#pragma once

#include <list>
#include <utility>

#include <core/StatefulService.h>

namespace SYSTEM::STATE {

template <typename UpdateHandler>
StateHandlerResult invokeUpdateHandlers(
    const std::list<UpdateHandler>& handlers,
    std::string_view originId) {
    for (const auto& handler : handlers) {
        const StateHandlerResult handlerResult = handler.callback(originId);
        if (!handlerResult.ok) {
            return handlerResult.errorCode
                ? handlerResult
                : StateHandlerResult::failure("internal/update_failed", handlerResult.httpStatus);
        }
    }

    return StateHandlerResult::success();
}

template <typename CallHandlersFn, typename RollbackFn>
StateTransactionResult finalizeStateTransaction(
    StateUpdateResult result,
    std::string_view originId,
    bool propagate,
    CallHandlersFn&& callHandlers,
    RollbackFn&& rollback,
    const char* rollbackErrorCode = "rtc/restore_failed") {
    if (!propagate || result != StateUpdateResult::CHANGED) {
        return StateTransactionResult::fromOutcome(result);
    }

    const StateHandlerResult handlerResult = callHandlers(originId);
    if (handlerResult.ok) {
        return StateTransactionResult::fromOutcome(result);
    }

    if (!rollback()) {
        return StateTransactionResult::failure(rollbackErrorCode);
    }

    return StateTransactionResult::failure(
        handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
        handlerResult.httpStatus);
}

}  // namespace SYSTEM::STATE
