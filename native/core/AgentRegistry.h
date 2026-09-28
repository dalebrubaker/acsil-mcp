// AgentRegistry.h — process-wide registry of chart agents and their pending requests.
//
// Agents (one per exposed chart, any chartbook) run on the Sierra Chart thread: they refresh their
// ChartInfo and drain their request queue on every study call. Socket threads call Call(), which
// queues a request for one chart and blocks until that chart's agent answers or the timeout expires.

#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace acsilmcp
{
    using Json = nlohmann::json;

    struct ChartInfo
    {
        std::string id;          // "<chartbook>#<chartNumber>", unique across chartbooks
        std::string chartbook;
        int chartNumber = 0;
        std::string name;
        std::string symbol;
        std::string timeZone;
        int chartDataType = 0;   // ChartDataTypeEnum
        int barPeriodType = 0;   // IntradayBarPeriodTypeEnum
        int barPeriodParam1 = 0; // seconds for IBPT_DAYS_MINS_SECS
        int barCount = 0;
        std::string firstBarTime; // chart-local "yyyy-MM-ddTHH:mm:ss", empty when no bars
        std::string lastBarTime;
    };

    Json ToJson(const ChartInfo& info);

    class AgentRegistry
    {
    public:
        using Clock = std::chrono::steady_clock;
        using Handler = std::function<Json(const Json& request)>;

        // Agent side (Sierra Chart thread).

        // Adds the agent or refreshes its info. Returns false when another agent (a different owner)
        // already holds this chart id; that agent keeps it.
        bool Upsert(const ChartInfo& info, int owner);

        // Removes the agent and fails its pending requests. No-op if `owner` does not hold `id`.
        void Remove(const std::string& id, int owner);

        // Answers every pending request for `id` with `handler`. A handler exception becomes an
        // error reply. Requests whose caller already timed out are skipped.
        void Drain(const std::string& id, const Handler& handler);

        // Server side (socket threads).

        Json ListCharts() const;

        // Queues `request` for chart `id` and waits for the agent's reply (a full reply object,
        // see docs/protocol.md). Returns an error reply for an unknown chart, a timeout, or an
        // agent that was removed while the request was pending.
        Json Call(const std::string& id, const Json& request, std::chrono::milliseconds timeout);

        std::size_t Count() const;

    private:
        struct Pending;

        struct Agent
        {
            ChartInfo info;
            int owner = 0;
            Clock::time_point lastUpdate;
            std::deque<std::shared_ptr<Pending>> queue;
        };

        mutable std::mutex mutex_;
        std::map<std::string, Agent> agents_;
    };

    Json ErrorReply(const std::string& message);
    Json OkReply(Json result);
}
