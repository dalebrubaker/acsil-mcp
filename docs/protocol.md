# Wire Protocol

AcsilMcp.dll listens on `127.0.0.1` (default port 22921, the agent study's Port input). The
acsil-mcp server connects as a client. Protocol version: **1** (`native/core/Version.h`,
`server/AcsilMcp.Server/ServerInfo.cs`).

## Framing

Every message is one frame: a 4-byte little-endian unsigned payload length, then that many bytes
of UTF-8 JSON. Payloads are limited to 64 MiB; a larger header closes the connection.

## Requests and replies

A client sends one request and waits for its reply before sending the next. Several clients may be
connected at once.

```json
{"id": 7, "cmd": "LIST_CHARTS"}
{"id": 7, "ok": true, "result": {"charts": [ ... ]}}
{"id": 8, "ok": false, "error": "no agent on chart Futures.Cht#9; call LIST_CHARTS for the available charts"}
```

- `id` is chosen by the client and echoed in the reply.
- `cmd` names the command.
- A request with a `chart` field (a chart id from `LIST_CHARTS`) is queued for that chart's agent
  and answered on Sierra Chart's thread the next time the agent study runs. If the agent does not
  run within the chart request timeout (study input, default 5 s), the reply is an error.
- Errors are replies with `ok: false`; the connection stays usable.

## Commands

### PING

Answered immediately. The server sends it after connecting and refuses a different
`protocolVersion`.

```json
{"protocolVersion": 1, "instanceId": "9f2c0b7e41d3a856", "agentCount": 2}
```

`instanceId` changes whenever the DLL's server restarts (for example after a DLL reload).

### LIST_CHARTS

Answered immediately from the agents' last reports.

```json
{"charts": [{
  "id": "Futures.Cht#3", "chartbook": "Futures.Cht", "chartNumber": 3,
  "name": "ESZ26 [CB] 1 Min #3", "symbol": "ESZ26", "timeZone": "America/New_York",
  "chartDataType": 2, "barPeriodType": 0, "barPeriodParam1": 60,
  "barCount": 1440, "firstBarTime": "2026-09-27T18:00:00", "lastBarTime": "2026-09-28T16:59:00",
  "lastUpdateAgeMs": 98, "pendingRequests": 0
}]}
```

- `chartDataType`, `barPeriodType`, and `barPeriodParam1` are ACSIL's `s_BarPeriod` fields
  (`ChartDataType`, `IntradayChartBarPeriodType`, `IntradayChartBarPeriodParameter1`).
- Times are chart-local wall-clock times, formatted `yyyy-MM-ddTHH:mm:ss`, in `timeZone`. They are
  empty strings when the chart has no bars.
- `lastUpdateAgeMs` is the time since the agent last ran. A large value means the chart is hidden
  or Sierra Chart is busy.
- `pendingRequests` counts requests waiting for that chart's agent.

Chart commands (bars, studies, fills, snapshot, scroll) arrive in later checkpoints; see
[PLAN.md](PLAN.md).
