#include "core/Bridge.h"

#include <random>

#include "core/Dispatcher.h"

namespace acsilmcp
{
    namespace
    {
        std::string NewInstanceId()
        {
            std::random_device device;
            const std::uint64_t value = (static_cast<std::uint64_t>(device()) << 32) | device();
            char text[17];
            snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
            return text;
        }
    }

    Bridge& Bridge::Instance()
    {
        static Bridge bridge;
        return bridge;
    }

    bool Bridge::AddOrUpdateAgent(const ChartInfo& info, int owner, const BridgeSettings& settings)
    {
        if (!registry_.Upsert(info, owner))
        {
            return false;
        }

        std::lock_guard lock(lifecycleMutex_);
        if (server_.IsRunning() || startFailed_)
        {
            // The first agent's settings win. A start failure is reported once and retried only
            // after every agent has left.
            return true;
        }

        settings_ = settings;
        instanceId_ = NewInstanceId();
        std::string error;
        const bool started = server_.Start(
            settings.port,
            [this](const std::string& payload)
            {
                return Dispatch(DispatchContext{registry_, instanceId_, settings_.chartTimeout}, payload);
            },
            [this](const std::string& message) { Log(message); },
            error);

        if (started)
        {
            Log("acsil-mcp: listening on 127.0.0.1:" + std::to_string(server_.Port()));
        }
        else
        {
            startFailed_ = true;
            Log("acsil-mcp: " + error);
        }
        return true;
    }

    void Bridge::RemoveAgent(const std::string& chartId, int owner)
    {
        registry_.Remove(chartId, owner);

        std::lock_guard lock(lifecycleMutex_);
        if (registry_.Count() == 0)
        {
            startFailed_ = false;
            if (server_.IsRunning())
            {
                server_.Stop();
                Log("acsil-mcp: stopped (no agents left)");
            }
        }
    }

    std::vector<std::string> Bridge::TakeLogs()
    {
        std::lock_guard lock(logMutex_);
        std::vector<std::string> logs;
        logs.swap(logs_);
        return logs;
    }

    void Bridge::Log(const std::string& message)
    {
        std::lock_guard lock(logMutex_);
        logs_.push_back(message);
    }
}
