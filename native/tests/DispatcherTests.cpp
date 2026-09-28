// DispatcherTests.cpp — Catch2 tests for core/Dispatcher.h.

#include <catch2/catch_test_macros.hpp>

#include "core/Dispatcher.h"
#include "core/Version.h"

using namespace acsilmcp;
using namespace std::chrono_literals;

namespace
{
    Json Send(AgentRegistry& registry, const std::string& payload)
    {
        return Json::parse(Dispatch(DispatchContext{registry, "abc123", 20ms}, payload));
    }
}

TEST_CASE("PING reports the protocol version and echoes the id", "[dispatch]")
{
    AgentRegistry registry;
    const Json reply = Send(registry, R"({"id":42,"cmd":"PING"})");

    REQUIRE(reply["ok"] == true);
    REQUIRE(reply["id"] == 42);
    REQUIRE(reply["result"]["protocolVersion"] == kProtocolVersion);
    REQUIRE(reply["result"]["instanceId"] == "abc123");
    REQUIRE(reply["result"]["agentCount"] == 0);
}

TEST_CASE("LIST_CHARTS returns registered charts", "[dispatch]")
{
    AgentRegistry registry;
    ChartInfo info;
    info.id = "A.Cht#2";
    info.chartbook = "A.Cht";
    info.chartNumber = 2;
    registry.Upsert(info, 1);

    const Json reply = Send(registry, R"({"id":1,"cmd":"LIST_CHARTS"})");
    REQUIRE(reply["ok"] == true);
    REQUIRE(reply["result"]["charts"].size() == 1);
    REQUIRE(reply["result"]["charts"][0]["chartNumber"] == 2);
}

TEST_CASE("Chart requests are routed to the registry", "[dispatch]")
{
    AgentRegistry registry;
    const Json reply = Send(registry, R"({"id":3,"cmd":"GET_BARS","chart":"Missing.Cht#1"})");
    REQUIRE(reply["ok"] == false);
    REQUIRE(reply["id"] == 3);
    REQUIRE(reply["error"].get<std::string>().find("Missing.Cht#1") != std::string::npos);
}

TEST_CASE("Malformed requests get error replies", "[dispatch]")
{
    AgentRegistry registry;
    REQUIRE(Send(registry, "not json")["ok"] == false);
    REQUIRE(Send(registry, R"({"id":1})")["ok"] == false);
    REQUIRE(Send(registry, R"({"id":1,"cmd":"NOPE"})")["error"] == "unknown command NOPE");
    REQUIRE(Send(registry, R"({"id":1,"cmd":"X","chart":5})")["ok"] == false);
}
