# Safety Boundary

acsil-mcp lets an AI assistant look at Sierra Chart charts. It must never be able to trade.

## Never

The DLL and the server must never:

- place, modify, replace, cancel, or transmit an order;
- flatten, reverse, or otherwise change a position;
- change the trade account, trading connection, or auto-trading state;
- change chart data, study settings, or chartbooks.

`scripts/check-no-trading.ps1` fails CI if the project's C++ sources reference ACSIL trading
APIs. It scans `native/core` and `native/adapter`; the Sierra Chart headers are excluded.

## Allowed mutations

Every mutation is a separately typed, reviewed MCP tool, never a generic command dispatcher.
Add a row here, with review, before exposing a new one.

| Tool | ACSIL | Effect |
|---|---|---|
| `move_chart_to_time` (planned, CP4) | `sc.ScrollToDateTime` on the chart's own agent | Scrolls the chart; changes nothing else |

## Exposure

- Only charts with the "ACSIL MCP Chart Agent" study are visible.
- The DLL listens on `127.0.0.1` only. The server speaks MCP over stdio. Nothing is exposed to
  the network.
