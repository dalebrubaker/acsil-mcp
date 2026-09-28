#include "core/AgentRegistry.h"

#include <atomic>
#include <future>
#include <vector>

namespace acsilmcp
{
    struct AgentRegistry::Pending
    {
        Json request;
        std::promise<Json> reply;
        std::atomic<bool> abandoned{false};
    };

    Json ErrorReply(const std::string& message)
    {
        return Json{{"ok", false}, {"error", message}};
    }

    Json OkReply(Json result)
    {
        return Json{{"ok", true}, {"result", std::move(result)}};
    }

    Json ToJson(const ChartInfo& info)
    {
        return Json{
            {"id", info.id},
            {"chartbook", info.chartbook},
            {"chartNumber", info.chartNumber},
            {"name", info.name},
            {"symbol", info.symbol},
            {"timeZone", info.timeZone},
            {"chartDataType", info.chartDataType},
            {"barPeriodType", info.barPeriodType},
            {"barPeriodParam1", info.barPeriodParam1},
            {"barCount", info.barCount},
            {"firstBarTime", info.firstBarTime},
            {"lastBarTime", info.lastBarTime},
        };
    }

    bool AgentRegistry::Upsert(const ChartInfo& info, int owner)
    {
        std::lock_guard lock(mutex_);
        auto [it, inserted] = agents_.try_emplace(info.id);
        if (!inserted && it->second.owner != owner)
        {
            return false;
        }
        it->second.info = info;
        it->second.owner = owner;
        it->second.lastUpdate = Clock::now();
        return true;
    }

    void AgentRegistry::Remove(const std::string& id, int owner)
    {
        std::deque<std::shared_ptr<Pending>> orphaned;
        {
            std::lock_guard lock(mutex_);
            const auto it = agents_.find(id);
            if (it == agents_.end() || it->second.owner != owner)
            {
                return;
            }
            orphaned.swap(it->second.queue);
            agents_.erase(it);
        }

        for (const auto& pending : orphaned)
        {
            pending->reply.set_value(ErrorReply("chart " + id + " was closed or its agent was removed"));
        }
    }

    void AgentRegistry::Drain(const std::string& id, const Handler& handler)
    {
        std::deque<std::shared_ptr<Pending>> work;
        {
            std::lock_guard lock(mutex_);
            const auto it = agents_.find(id);
            if (it == agents_.end())
            {
                return;
            }
            work.swap(it->second.queue);
        }

        for (const auto& pending : work)
        {
            if (pending->abandoned.load())
            {
                continue;
            }

            Json reply;
            try
            {
                reply = handler(pending->request);
            }
            catch (const std::exception& ex)
            {
                reply = ErrorReply(std::string("agent failed: ") + ex.what());
            }
            pending->reply.set_value(std::move(reply));
        }
    }

    Json AgentRegistry::ListCharts() const
    {
        const auto now = Clock::now();
        Json charts = Json::array();
        std::lock_guard lock(mutex_);
        for (const auto& [id, agent] : agents_)
        {
            Json chart = ToJson(agent.info);
            chart["lastUpdateAgeMs"] =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - agent.lastUpdate).count();
            chart["pendingRequests"] = agent.queue.size();
            charts.push_back(std::move(chart));
        }
        return charts;
    }

    Json AgentRegistry::Call(const std::string& id, const Json& request, std::chrono::milliseconds timeout)
    {
        auto pending = std::make_shared<Pending>();
        pending->request = request;
        auto reply = pending->reply.get_future();
        {
            std::lock_guard lock(mutex_);
            const auto it = agents_.find(id);
            if (it == agents_.end())
            {
                return ErrorReply("no agent on chart " + id + "; call LIST_CHARTS for the available charts");
            }
            it->second.queue.push_back(pending);
        }

        if (reply.wait_for(timeout) != std::future_status::ready)
        {
            pending->abandoned = true;
            return ErrorReply("chart " + id + " did not respond within " + std::to_string(timeout.count()) +
                              " ms; the chart may be hidden or Sierra Chart busy");
        }
        return reply.get();
    }

    std::size_t AgentRegistry::Count() const
    {
        std::lock_guard lock(mutex_);
        return agents_.size();
    }
}
