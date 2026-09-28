// AgentRegistryTests.cpp — Catch2 tests for core/AgentRegistry.h.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <future>
#include <thread>

#include "core/AgentRegistry.h"

using namespace acsilmcp;
using namespace std::chrono_literals;

namespace
{
    ChartInfo MakeChart(const std::string& chartbook, int number)
    {
        ChartInfo info;
        info.chartbook = chartbook;
        info.chartNumber = number;
        info.id = chartbook + "#" + std::to_string(number);
        info.symbol = "ESZ26";
        return info;
    }

    // Runs Drain on another thread until the reply future is ready, standing in for SC calls.
    template <typename F>
    Json CallWhileDraining(AgentRegistry& registry, const std::string& id, F handler, std::chrono::milliseconds timeout)
    {
        std::atomic<bool> done{false};
        std::thread agent([&]
        {
            while (!done)
            {
                registry.Drain(id, handler);
                std::this_thread::sleep_for(1ms);
            }
        });
        Json reply = registry.Call(id, Json{{"cmd", "X"}, {"chart", id}}, timeout);
        done = true;
        agent.join();
        return reply;
    }
}

TEST_CASE("Agents from different chartbooks are all listed", "[registry]")
{
    AgentRegistry registry;
    REQUIRE(registry.Upsert(MakeChart("Futures.Cht", 1), 1));
    REQUIRE(registry.Upsert(MakeChart("Stocks.Cht", 1), 1));

    const Json charts = registry.ListCharts();
    REQUIRE(charts.size() == 2);
    REQUIRE(charts[0]["id"] == "Futures.Cht#1");
    REQUIRE(charts[1]["id"] == "Stocks.Cht#1");
    REQUIRE(charts[0].contains("lastUpdateAgeMs"));
    REQUIRE(charts[0]["pendingRequests"] == 0);
}

TEST_CASE("A second agent on the same chart is refused", "[registry]")
{
    AgentRegistry registry;
    REQUIRE(registry.Upsert(MakeChart("A.Cht", 3), 7));
    REQUIRE_FALSE(registry.Upsert(MakeChart("A.Cht", 3), 8));
    REQUIRE(registry.Upsert(MakeChart("A.Cht", 3), 7));

    registry.Remove("A.Cht#3", 8);
    REQUIRE(registry.Count() == 1);
}

TEST_CASE("Call returns the agent's reply", "[registry]")
{
    AgentRegistry registry;
    registry.Upsert(MakeChart("A.Cht", 1), 1);

    const Json reply = CallWhileDraining(registry, "A.Cht#1",
        [](const Json& request) { return OkReply(Json{{"echo", request["cmd"]}}); }, 2000ms);

    REQUIRE(reply["ok"] == true);
    REQUIRE(reply["result"]["echo"] == "X");
}

TEST_CASE("A handler exception becomes an error reply", "[registry]")
{
    AgentRegistry registry;
    registry.Upsert(MakeChart("A.Cht", 1), 1);

    const Json reply = CallWhileDraining(registry, "A.Cht#1",
        [](const Json&) -> Json { throw std::runtime_error("boom"); }, 2000ms);

    REQUIRE(reply["ok"] == false);
    REQUIRE(reply["error"].get<std::string>().find("boom") != std::string::npos);
}

TEST_CASE("Call to an unknown chart fails fast", "[registry]")
{
    AgentRegistry registry;
    const Json reply = registry.Call("Nope.Cht#9", Json{{"cmd", "X"}}, 5000ms);
    REQUIRE(reply["ok"] == false);
    REQUIRE(reply["error"].get<std::string>().find("Nope.Cht#9") != std::string::npos);
}

TEST_CASE("Call times out when the agent never drains, and a late drain skips it", "[registry]")
{
    AgentRegistry registry;
    registry.Upsert(MakeChart("A.Cht", 1), 1);

    const Json reply = registry.Call("A.Cht#1", Json{{"cmd", "X"}}, 20ms);
    REQUIRE(reply["ok"] == false);
    REQUIRE(reply["error"].get<std::string>().find("did not respond") != std::string::npos);

    int handled = 0;
    registry.Drain("A.Cht#1", [&](const Json&) { ++handled; return OkReply(Json::object()); });
    REQUIRE(handled == 0);
}

TEST_CASE("Removing an agent fails its pending requests", "[registry]")
{
    AgentRegistry registry;
    registry.Upsert(MakeChart("A.Cht", 1), 1);

    auto pending = std::async(std::launch::async,
        [&] { return registry.Call("A.Cht#1", Json{{"cmd", "X"}}, 5000ms); });
    while (registry.ListCharts()[0]["pendingRequests"] == 0)
    {
        std::this_thread::sleep_for(1ms);
    }

    const auto start = std::chrono::steady_clock::now();
    registry.Remove("A.Cht#1", 1);
    const Json reply = pending.get();

    REQUIRE(reply["ok"] == false);
    REQUIRE(reply["error"].get<std::string>().find("closed") != std::string::npos);
    REQUIRE(std::chrono::steady_clock::now() - start < 1000ms);
    REQUIRE(registry.Count() == 0);
}
