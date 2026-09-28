// AgentStudy.cpp — "ACSIL MCP Chart Agent" study.
//
// Add one instance to each chart that the acsil-mcp server may see. Charts without an agent are
// invisible to the assistant. The agent reads its own chart and never trades: see docs/safety.md.
//
// On every call (UpdateAlways) the agent refreshes its chart's ChartInfo in the process-wide
// Bridge, answers the requests queued for its chart, and flushes bridge messages to the SC log.
// The first agent starts the loopback server; the last one to be removed stops it.

#include "sierrachart.h"

#include <chrono>
#include <cstdio>
#include <string>

#include "core/Bridge.h"

namespace
{
    using acsilmcp::Bridge;
    using acsilmcp::ChartInfo;
    using acsilmcp::Json;

    constexpr int kStateKey = 1;

    struct AgentState
    {
        std::string chartId;
        bool registered = false;
        bool duplicateReported = false;
        bool portMismatchReported = false;
    };

    std::string FormatDateTime(const SCDateTime& dateTime)
    {
        int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
        dateTime.GetDateTimeYMDHMS(year, month, day, hour, minute, second);
        char text[32];
        snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d:%02d", year, month, day, hour, minute, second);
        return text;
    }

    ChartInfo ReadChartInfo(SCStudyInterfaceRef sc)
    {
        ChartInfo info;
        info.chartbook = sc.ChartbookName().GetChars();
        info.chartNumber = sc.ChartNumber;
        info.id = info.chartbook + "#" + std::to_string(sc.ChartNumber);
        info.name = sc.GetChartName(sc.ChartNumber).GetChars();
        info.symbol = sc.Symbol.GetChars();
        info.timeZone = sc.GetChartTimeZone(sc.ChartNumber).GetChars();

        n_ACSIL::s_BarPeriod period;
        sc.GetBarPeriodParameters(period);
        info.chartDataType = period.ChartDataType;
        info.barPeriodType = period.IntradayChartBarPeriodType;
        info.barPeriodParam1 = period.IntradayChartBarPeriodParameter1;

        info.barCount = sc.ArraySize;
        if (sc.ArraySize > 0)
        {
            info.firstBarTime = FormatDateTime(sc.BaseDateTimeIn[0]);
            info.lastBarTime = FormatDateTime(sc.BaseDateTimeIn[sc.ArraySize - 1]);
        }
        return info;
    }

    // Answers one request routed to this chart. CP1 has no chart commands yet; CP2 adds bars and studies.
    Json HandleChartRequest(SCStudyInterfaceRef, const Json& request)
    {
        return acsilmcp::ErrorReply("unknown chart command " + request.value("cmd", std::string()));
    }

    void FlushLogs(SCStudyInterfaceRef sc)
    {
        for (const auto& message : Bridge::Instance().TakeLogs())
        {
            sc.AddMessageToLog(message.c_str(), 0);
        }
    }

    void Run(SCStudyInterfaceRef sc)
    {
        SCInputRef portInput = sc.Input[0];
        SCInputRef timeoutInput = sc.Input[1];

        if (sc.SetDefaults)
        {
            sc.GraphName = "ACSIL MCP Chart Agent";
            sc.StudyDescription = "Exposes this chart to the acsil-mcp MCP server (read-only, never trades).";
            sc.AutoLoop = 0;
            sc.GraphRegion = 0;
            sc.UpdateAlways = 1;

            portInput.Name = "Port (127.0.0.1)";
            portInput.SetInt(22921);
            portInput.SetIntLimits(1024, 65535);
            portInput.SetDescription("Loopback port the acsil-mcp server connects to. The first agent to start sets it for all charts.");

            timeoutInput.Name = "Chart Request Timeout (ms)";
            timeoutInput.SetInt(5000);
            timeoutInput.SetIntLimits(100, 60000);
            timeoutInput.SetDescription("How long a request waits for a chart's agent to answer.");
            return;
        }

        auto* state = static_cast<AgentState*>(sc.GetPersistentPointer(kStateKey));

        if (sc.LastCallToFunction)
        {
            if (state != nullptr)
            {
                if (state->registered)
                {
                    Bridge::Instance().RemoveAgent(state->chartId, sc.StudyGraphInstanceID);
                }
                delete state;
                sc.SetPersistentPointer(kStateKey, nullptr);
            }
            FlushLogs(sc);
            return;
        }

        if (state == nullptr)
        {
            state = new AgentState();
            sc.SetPersistentPointer(kStateKey, state);
        }

        const ChartInfo info = ReadChartInfo(sc);
        auto& bridge = Bridge::Instance();

        // A renamed chartbook changes the chart id: leave under the old id first.
        if (state->registered && state->chartId != info.id)
        {
            bridge.RemoveAgent(state->chartId, sc.StudyGraphInstanceID);
            state->registered = false;
        }

        acsilmcp::BridgeSettings settings;
        settings.port = static_cast<std::uint16_t>(portInput.GetInt());
        settings.chartTimeout = std::chrono::milliseconds(timeoutInput.GetInt());

        state->registered = bridge.AddOrUpdateAgent(info, sc.StudyGraphInstanceID, settings);
        state->chartId = info.id;

        if (!state->registered)
        {
            if (!state->duplicateReported)
            {
                sc.AddMessageToLog("acsil-mcp: this chart already has an ACSIL MCP Chart Agent; this one is inactive.", 1);
                state->duplicateReported = true;
            }
            FlushLogs(sc);
            return;
        }

        if (bridge.Port() != 0 && bridge.Port() != settings.port && !state->portMismatchReported)
        {
            sc.AddMessageToLog(("acsil-mcp: the server already listens on port " + std::to_string(bridge.Port()) +
                                "; this agent's Port input is ignored.").c_str(), 0);
            state->portMismatchReported = true;
        }

        bridge.Registry().Drain(info.id, [&sc](const Json& request) { return HandleChartRequest(sc, request); });
        FlushLogs(sc);
    }
}

SCSFExport scsf_AcsilMcpChartAgent(SCStudyInterfaceRef sc)
{
    try
    {
        Run(sc);
    }
    catch (const std::exception& ex)
    {
        sc.AddMessageToLog((std::string("acsil-mcp: agent error: ") + ex.what()).c_str(), 1);
    }
}
