# acsil-mcp

An [MCP](https://modelcontextprotocol.io) server that lets AI assistants (Claude Code, Claude
Desktop, Codex, Cursor, ...) read and navigate the charts that are **open in Sierra Chart right
now**: list charts, see the bars on screen, read study values, read fills, take a snapshot, and
scroll a chart to a date. It never trades.

> **Status: pre-alpha.** Only `list_charts` is implemented. See [docs/PLAN.md](docs/PLAN.md).

Sierra Chart is a trademark of its owner. This project is not affiliated with or endorsed by
Sierra Chart.

## How it works

```text
MCP client ──stdio──► acsil-mcp server (.NET 10)
                          │ loopback TCP 127.0.0.1
                          ▼
AcsilMcp.dll in Sierra Chart ◄── "ACSIL MCP Chart Agent" study on each exposed chart
```

The agent study runs inside each chart, so it knows what you see: the visible range, the bar
under the cursor, and the chart's fills. Charts without an agent are invisible to the assistant.
See [docs/safety.md](docs/safety.md).

## Building

Requirements: Visual Studio 2022 (Desktop development with C++, v143, vcpkg), .NET 10 SDK.

1. Put Sierra Chart's ACSIL headers in `native/ACS_Source` (see
   [native/ACS_Source/README.md](native/ACS_Source/README.md)), or point `AcsSourceDir` at your
   installation's `ACS_Source` folder.
2. Optional: copy `native/AcsilMcp.user.props.example` to `native/AcsilMcp.user.props` and list your
   Sierra Chart installations. Each build then asks them to release study DLLs, copies
   `AcsilMcp.dll` into their `Data` folder, and lets them load it again (needs the Sierra Chart UDP
   port enabled under Global Settings >> Sierra Chart Server Settings).
3. Open `AcsilMcp.sln` and build `Debug|x64` or `Release|x64`, or:

```powershell
msbuild AcsilMcp.sln -restore -p:Configuration=Release -p:Platform=x64
```

Pass `-p:SkipSierraChartDeploy=true` to build without touching running Sierra Chart instances.

## Using it

1. In Sierra Chart, add the study **ACSIL MCP Chart Agent** (Analysis >> Studies >> Add Custom
   Study >> AcsilMcp) to each chart the assistant may see. Its Port input defaults to 22921.
2. Register the server with your MCP client. For Claude Code:

```powershell
claude mcp add acsil-mcp -- dotnet E:\path\to\acsil-mcp\server\AcsilMcp.Server\bin\Release\net10.0\acsil-mcp.dll
```

   Add `--port N` after the DLL path (or set `ACSIL_MCP_PORT`) if you changed the study's Port.
3. Ask about your charts. The wire protocol is in [docs/protocol.md](docs/protocol.md).

## License

MIT. See [LICENSE](LICENSE).
