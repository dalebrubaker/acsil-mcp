// Version.h — wire protocol version shared by the AcsilMcp DLL and the acsil-mcp server.
//
// core/ never includes sierrachart.h, so it builds and is unit-tested without Sierra Chart.
// The server's companion constant is AcsilMcp.Server.ServerInfo.ProtocolVersion; bump both together.

#pragma once

namespace acsilmcp
{
    inline constexpr int kProtocolVersion = 1;
}
