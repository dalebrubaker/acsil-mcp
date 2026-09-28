// Bridge.h — process-wide owner of the registry and the socket server.
//
// A study DLL is loaded once per Sierra Chart process, so this singleton is shared by agents in
// every chartbook. The first agent to register starts the server with its port and timeout; the
// last agent to leave stops it, so a DLL release (RELEASE_ALL_DLLS) never leaves threads behind.

#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "core/AgentRegistry.h"
#include "core/SocketServer.h"

namespace acsilmcp
{
    struct BridgeSettings
    {
        std::uint16_t port = 22921;
        std::chrono::milliseconds chartTimeout{5000};
    };

    class Bridge
    {
    public:
        static Bridge& Instance();

        // Registers or refreshes an agent, starting the server when it is the first. Returns false
        // when another agent already owns this chart id.
        bool AddOrUpdateAgent(const ChartInfo& info, int owner, const BridgeSettings& settings);

        // Unregisters an agent (failing its pending requests) and stops the server with the last.
        void RemoveAgent(const std::string& chartId, int owner);

        AgentRegistry& Registry() { return registry_; }

        // Messages for the Sierra Chart message log. Agents flush them on the SC thread.
        std::vector<std::string> TakeLogs();

        // The port actually listened on, or 0 when the server is not running.
        std::uint16_t Port() const { return server_.Port(); }

    private:
        void Log(const std::string& message);

        AgentRegistry registry_;
        SocketServer server_;
        std::mutex lifecycleMutex_;
        std::string instanceId_;
        BridgeSettings settings_;
        bool startFailed_ = false;
        std::mutex logMutex_;
        std::vector<std::string> logs_;
    };
}
