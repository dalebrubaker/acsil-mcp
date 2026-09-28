// Dispatcher.h — turns one request payload into one reply payload (docs/protocol.md).
//
// PING and LIST_CHARTS are answered on the socket thread from the registry. Any request that names
// a "chart" is routed to that chart's agent and answered on the Sierra Chart thread.

#pragma once

#include <chrono>
#include <string>

#include "core/AgentRegistry.h"

namespace acsilmcp
{
    struct DispatchContext
    {
        AgentRegistry& registry;
        std::string instanceId;
        std::chrono::milliseconds chartTimeout;
    };

    std::string Dispatch(const DispatchContext& context, const std::string& requestPayload);
}
