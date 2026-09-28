// AgentStudy.cpp — "ACSIL MCP Chart Agent" study.
//
// Add one instance to each chart that the acsil-mcp server may see. Charts without an agent are
// invisible to the assistant. The agent reads its own chart and never trades: see docs/safety.md.
//
// Stage 0 skeleton: registers the study only. The request queue and socket thread arrive in CP1.

#include "sierrachart.h"

#include "core/Version.h"

SCSFExport scsf_AcsilMcpChartAgent(SCStudyInterfaceRef sc)
{
    if (sc.SetDefaults)
    {
        sc.GraphName = "ACSIL MCP Chart Agent";
        sc.StudyDescription = "Exposes this chart to the acsil-mcp MCP server (read-only, never trades).";
        sc.AutoLoop = 0;
        sc.GraphRegion = 0;
        return;
    }
}
