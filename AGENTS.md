# Repository Guidelines

Guidance for AI coding assistants working in this repository. `CLAUDE.md` imports this file.

## Scope and Safety

- Read [docs/PLAN.md](docs/PLAN.md) for the architecture and the active checkpoint.
- acsil-mcp must never trade. Read [docs/safety.md](docs/safety.md) before adding any ACSIL call or
  MCP tool. Never call ACSIL order, position, or trade-account functions.
- Each chart mutation is its own typed MCP tool, added to `docs/safety.md` first. Never add a generic
  command dispatcher.

## Layout

- `native/core/` — protocol, registry, JSON; never includes `sierrachart.h`; unit-tested.
- `native/adapter/` — the ACSIL study; the only code that touches `sc`.
- `native/ACS_Source/` — Sierra Chart headers; do not modify.
- `server/` — .NET 10 MCP server (stdio) and its tests.
- `tools/ScUdpCommand/` — sends RELEASE_ALL_DLLS / ALLOW_LOAD_ALL_DLLS to Sierra Chart.

## Build and Test

- Full build: `msbuild AcsilMcp.sln -restore -p:Configuration=Release -p:Platform=x64`.
- **Agents always pass `-p:SkipSierraChartDeploy=true`.** A deploying build unloads every study DLL
  in the running Sierra Chart instances listed in `native/AcsilMcp.user.props`, which interrupts
  anything else those instances run. Deploy only when the maintainer asks.
- Native tests: `native\x64\<Config>\Tests\AcsilMcpTests.exe`.
- Server tests: `dotnet test server\AcsilMcp.Server.Tests`.
- Safety guard: `pwsh scripts\check-no-trading.ps1`.
- Live checks inside Sierra Chart are run by the maintainer.

## Looking Up ACSIL

Check a capability against both sources before relying on it:

1. **Headers** — `native/ACS_Source/sierrachart.h` declares every `sc.` member and function, with
   exact signatures. Search it first.
2. **Documentation** — semantics (which chart a member applies to, when a write takes effect,
   threading) are only in Sierra Chart's docs. The main pages:
   - Members: <https://www.sierrachart.com/index.php?page=doc/ACSIL_Members_Variables_And_Arrays.html>
   - Functions: <https://www.sierrachart.com/index.php?page=doc/ACSIL_Members_Functions.html>
   - Other charts and time frames: <https://www.sierrachart.com/index.php?page=doc/ACSILRefOtherTimeFrames.php>
   - Trading (read-only use here): <https://www.sierrachart.com/index.php?page=doc/ACSILTrading.html>
   - Drawings: <https://www.sierrachart.com/index.php?page=doc/ACSILDrawingTools.html>

   The pages are large. Run `pwsh scripts\fetch-acsil-docs.ps1` once. It downloads every ACSIL page
   to `docs\acsil-cache\*.txt` (git-ignored, Sierra Chart's text, never commit it; `-Force`
   refreshes). Search that folder for the `### sc.Name` heading rather than asking a summarizer.

Record verified findings, with the date and header `SC_DLL_VERSION`, in the "Verified ACSIL Surface"
table in `docs/PLAN.md`.

## Code Style

- C++20, v143, `/MT`, MultiByte, four-space indentation, braces on every control-flow body.
- C#: net10.0, file-scoped namespaces, nullable enabled.
- Keep wire constants in sync across `native/core` and `server/` and test both sides.

## Git

- Commit only with the maintainer's approval. Compact one-line commit subjects.
- Never commit `native/AcsilMcp.user.props`.
