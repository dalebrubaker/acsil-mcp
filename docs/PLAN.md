# acsil-mcp — Implementation Plan

Status: CP0 (repository bootstrap) built locally; awaiting commit and CI

## Objective

An open-source [Model Context Protocol](https://modelcontextprotocol.io) server that lets an AI
assistant (Claude Code, Claude Desktop, Codex, Cursor, ...) read and navigate the charts that are
**open in Sierra Chart right now**: list charts, see the bars that are on screen, read study values,
read fills, take a snapshot, and scroll a chart to a date. It can never trade.

What distinguishes it from existing Sierra Chart MCP projects (file-exporter and configuration
bridges): it answers live requests from inside the running charts, and it knows what the user is
looking at — visible range, cursor bar, and scroll position.

Sierra Chart is a trademark of its owner; this project is not affiliated with or endorsed by it.

## Architecture

```text
MCP client ──stdio──► acsil-mcp server (.NET 10, ModelContextProtocol SDK)
                          │  loopback TCP 127.0.0.1:22921, length-prefixed JSON
                          ▼
AcsilMcp.dll (loaded once by Sierra Chart)
  process-global agent registry + socket thread
     ▲ enqueue request for chart K, wait with timeout
     │
  "ACSIL MCP Chart Agent" study — one per exposed chart, in any chartbook
     runs on the SC thread each update (UpdateAlways = 1), drains its queue,
     reads its own chart, fulfils the reply
```

Decisions:

1. **Per-chart agent study, opt-in.** The user adds the agent study to each chart to expose. It is
   required: the visible range, cursor, fills, and `ScrollToDateTime` are members of the *calling*
   study's `sc`, valid only for its own chart (see Verified ACSIL Surface). Opt-in is also the
   privacy boundary — charts without an agent are invisible to the assistant.
2. **Process-global registry spans chartbooks.** `GetHighestChartNumberUsedInChartBook` covers only
   the caller's chartbook, but a study DLL is loaded once per Sierra Chart process, so its statics
   are shared by all chartbooks. Chart identity is `chartbook + chart number`; the display name is
   `"<chartbook> #<n> <symbol> <period>"`. The first agent to register starts the socket thread;
   the last to unregister stops it.
3. **Thin C++, shaping in C#.** C++ returns raw index-range data, hard-capped per request. The
   server does downsampling (LTTB), defaults such as `visibleOnly`, null rules, PNG resizing, and
   all tool descriptions, so tool changes never require reloading the DLL into Sierra Chart.
4. **Loopback TCP with length-prefixed JSON, no messaging library.** Winsock on the C++ side and
   `TcpClient` on the .NET side. No libzmq to ship, and it works under Wine. Bind `127.0.0.1` only;
   port is a study input (default 22921).
5. **Core/adapter split in C++.** `core/` (registry, framing, JSON, request validation) never
   includes `sierrachart.h` and is unit-tested in CI. `adapter/` holds the ACSIL study and is the
   only code that touches `sc`.
6. **ACSIL headers live in `native/ACS_Source`.** The maintainer chose to commit Sierra Chart's
   `.h` files there, as BruScCpp does, so CI can build the DLL; that commit is made by the
   maintainer. `AcsSourceDir` (default `native\ACS_Source`) points the build at another folder,
   such as an installation's `ACS_Source`. CI builds the DLL only when `sierrachart.h` is present.
7. **Build and deploy like BruScCpp.** `AcsilMcp.sln` holds the DLL, its Catch2 tests,
   `ScUdpCommand`, and the .NET projects. Before a build, each Sierra Chart instance listed in the
   git-ignored `native\AcsilMcp.user.props` is sent `RELEASE_ALL_DLLS` over UDP. After it, the DLL
   and PDB are copied to `<instance>\Data` and `ALLOW_LOAD_ALL_DLLS` is sent.
   `-p:SkipSierraChartDeploy=true` disables both steps.
8. **Distribution.** GitHub Releases attach `AcsilMcp.dll` and a self-contained
   `acsil-mcp.exe`. The server is also published as a `dotnet tool` (`dotnet tool install -g
   acsil-mcp`).

## Verified ACSIL Surface

Verified 2026-09-28 against `sierrachart.h` (`SC_DLL_VERSION` 2876 and 2882) and the
sierrachart.com ACSIL docs.

| Need | ACSIL | Notes |
|---|---|---|
| Enumerate charts | `GetHighestChartNumberUsedInChartBook()`, `GetChartName`, `GetChartSymbol`, `ChartbookName()`, `GetChartTimeZone` | Caller's chartbook only; covered by decision 2 |
| Bars | `GetChartBaseData(chartNum, SCGraphData&)` | |
| Visible range | `IndexOfFirstVisibleBar` / `IndexOfLastVisibleBar` | Read-only, own chart; needs `UpdateAlways = 1` to track scrolling |
| Studies | `GetStudyCount`, `GetStudyIDByIndex`, `GetChartStudyShortName`, `GetStudyNameFromChart`, `GetStudySubgraphNameFromChart`, `GetStudyArraysFromChartUsingID`, `GetStudyDataStartIndexFromChartUsingID` | Any chart number in the caller's chartbook |
| Study inputs | `GetChartStudyInputType` / `Int` / `Float` / `String` | |
| Cursor bar | `ActiveToolIndex` | Only kept current when `ReceivePointerEvents` is set and the Pointer, Chart Values, or Hand tool is selected |
| Scroll | `ScrollToDateTime` (read/write `SCDateTime` member) | Own chart; applied after the study function returns, then cleared. Screen placement of the date is undocumented |
| Snapshot | `SaveChartImageToFileExtended(chartNum, path, w, h, includeOverlays)` | Asynchronous (after return); last call wins; nonzero `w`/`h` resizes the user's chart, so always pass 0,0; hidden charts need `includeOverlays = 0` |
| Fills / position | `GetOrderFillArraySize`, `GetOrderFillEntry`, `GetTradePosition` | Own chart's symbol and trade account only; fills load asynchronously after a reload |
| Drawings | `GetUserDrawnChartDrawing(chartNum, type, s_UseTool&, index)` | |
| Zoom | `ChartBarSpacing` (writable) | Out of scope for v1 |

Semantic gap: Sierra Chart subgraphs hold `0` where nothing is computed. Report `null` only before
the study's data start index and at the still-forming last bar, and document that a `0` may mean
"no value".

Times are reported as chart-local wall-clock ISO strings plus the chart's time zone name. No UTC
conversion.

## MCP Tools (v1)

| Tool | Checkpoint | Purpose |
|---|---|---|
| `list_charts` | CP1 | Charts with an agent: identity, symbol, period, bar count, time zone |
| `get_chart_summary` | CP2/CP3 | Visible and loaded range, last price, study names, cursor bar with every study's value there |
| `get_chart_bars` | CP2 | OHLCV, `visibleOnly = true` by default, `maxBars` with LTTB beyond it |
| `get_chart_studies` | CP2 | Subgraph series (and optionally inputs) for the same range |
| `get_chart_fills` | CP3 | Fills and position for the chart's symbol and trade account |
| `get_chart_snapshot` | CP3 | PNG, resized server-side to `maxWidth` |
| `move_chart_to_time` | CP4 | The only mutation: scroll; returns the resulting visible range |
| `get_chart_drawings` | CP5 | User-drawn tools as data |

Failures return `{"error": "..."}` naming the chart and the reason (for example "agent did not
respond within 5 s — chart hidden or Sierra Chart busy").

## Safety

- The DLL calls no order, position-changing, or trade-account-changing ACSIL function, and never
  writes `SelectedTradeAccount` or auto-trading members. `docs/safety.md` states the boundary.
- CI source guard: a script fails the build if C++ sources contain any of
  `BuyEntry|SellEntry|BuyExit|SellExit|BuyOrder|SellOrder|SubmitOrder|ModifyOrder|CancelOrder|CancelAllOrders|FlattenPosition|FlattenAndCancel|SelectedTradeAccount|SendOrdersToTradeService`.
- Every mutation is a separately typed, reviewed tool; never a generic command dispatcher. v1 has
  exactly one (`move_chart_to_time`).
- No network exposure: loopback only, stdio MCP transport, no remote hosting in v1.

## Repository Layout

```text
acsil-mcp/
  AcsilMcp.sln
  README.md  AGENTS.md  CLAUDE.md  LICENSE  SECURITY.md  CONTRIBUTING.md  CHANGELOG.md
  docs/  PLAN.md  safety.md  (later: install.md  protocol.md  tools.md)
  native/
    AcsilMcp.vcxproj  AcsilMcpTests.vcxproj  vcpkg.json (catch2; nlohmann-json from CP1)
    AcsilMcp.user.props.example   per-machine deploy targets (copy to AcsilMcp.user.props)
    ACS_Source/  Sierra Chart headers
    core/        registry, framing, json, validation — no sierrachart.h
    adapter/     DllName.cpp, AgentStudy.cpp (scsf_AcsilMcpChartAgent)
    tests/       Catch2 tests for core/
  server/
    AcsilMcp.Server/        .NET 10 MCP server (stdio), assembly acsil-mcp
    AcsilMcp.Server.Tests/  xUnit; fake agent over loopback TCP
  tools/ScUdpCommand/       RELEASE_ALL_DLLS / ALLOW_LOAD_ALL_DLLS sender used by the build
  scripts/check-no-trading.ps1
  .github/workflows/  ci.yml  (CP4: release.yml)
```

## Checkpoints

Each checkpoint ends with build + tests and a live check in Sierra Chart run by the maintainer.

### CP0 — Repository bootstrap

Public GitHub repo, license, README skeleton with status and disclaimer, `.gitignore`
(VS/.NET/vcpkg), layout above with empty projects that build, `ci.yml` (windows-latest: vcpkg +
MSBuild for `core/` tests, `dotnet test`, source guard). Acceptance: CI green on `main`.

Local result (2026-09-28): Debug and Release build against the `SC_DLL_VERSION` 2882 headers.
The DLL exports `scdll_DLLName`, `scdll_DLLVersion`, and `scsf_AcsilMcpChartAgent`. Native and
server tests pass. The guard passes clean and fails on a planted `sc.BuyEntry`. A deploy dry run
to a scratch folder sent both UDP commands and copied the DLL and PDB. Remaining: commit the
headers and the skeleton, CI green on `main`, and a live check that the study appears in Sierra
Chart's Add Custom Study list.

### CP1 — Transport skeleton and `list_charts`

- C++: registry (per-agent queue, promise with timeout, fail-all on unregister), framing, socket
  thread lifetime tied to agent refcount, `PING` and `LIST_CHARTS`. Study inputs: port, timeout.
- Server: TCP client with reconnect, `list_charts`.
- Tests: Catch2 for registry (enqueue/fulfil/timeout/agent removed mid-request) and framing; xUnit
  against a fake agent.
- Live: agents in two chartbooks both listed; Sierra Chart unaffected when the server is absent;
  DLL release/reload stops and restarts the thread cleanly; measure latency against the chart
  update interval.

### CP2 — Summary, bars, studies

- C++: `GET_SUMMARY`, `GET_BARS first last`, `GET_STUDIES first last [inputs]`; cap 50,000 bars.
- Server: `get_chart_summary`, `get_chart_bars`, `get_chart_studies`; LTTB; null rules.
- Live: visible range matches the screen after scrolling; study values match the Chart Values
  window at three bars.

### CP3 — Fills, snapshot, cursor

- C++: `GET_FILLS`, `GET_POSITION`, `SNAPSHOT path` (0,0); cursor via `ActiveToolIndex` with
  `ReceivePointerEvents` enabled.
- Server: `get_chart_fills`, `get_chart_snapshot` (waits for the PNG to appear and stabilise, then
  resizes), cursor block in `get_chart_summary`.
- Live: snapshot does not resize or flicker the chart; pointer events do not change mouse,
  drawing-tool, or chart-trading behaviour (if they do, drop the cursor feature); fills match the
  Trade Activity Log.

### CP4 — `move_chart_to_time`, docs, v0.1.0

- `docs/safety.md` entry first, then `SCROLL_TO` (sets `ScrollToDateTime`; reply sent on the
  agent's next call with the resulting visible range) and the tool.
- Live check settles where the date lands on screen; document it in the tool description.
  Date-only input resolves to the end of that day's session.
- `install.md` (DLL install, study setup, client config for Claude Code / Claude Desktop / Codex),
  `tools.md`, `release.yml`, tag `v0.1.0`.

### CP5 — Optional

`get_chart_drawings`; zoom via `ChartBarSpacing` (new safety review first).

## Risks

- **Study not called while a chart is hidden or Sierra Chart is idle.** Requests time out; the
  error names the chart. Verify in CP1.
- **DLL reload mid-request.** The registry fails all pending requests on the last agent's
  `LastCallToFunction` before joining the thread.
- **Header drift.** A DLL built against old headers may lack newer `sc` members. Refresh
  `native/ACS_Source` from a current installation when a checkpoint needs a new member.
- **Payload size.** Keep the C++ cap and the `maxBars` default (1000) aligned; document costs in
  `tools.md`.
