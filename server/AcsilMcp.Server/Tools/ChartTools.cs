using System.ComponentModel;
using System.Text.Json;
using Microsoft.Extensions.Logging;
using ModelContextProtocol.Server;

namespace AcsilMcp.Server.Tools;

public sealed record ChartSummary(
    string Id,
    string Chartbook,
    int ChartNumber,
    string Name,
    string Symbol,
    string BarPeriod,
    string TimeZone,
    int BarCount,
    string? FirstBarTime,
    string? LastBarTime,
    long LastUpdateAgeMs,
    int PendingRequests);

public sealed record ListChartsResult(IReadOnlyList<ChartSummary> Charts);

[McpServerToolType]
public sealed class ChartTools
{
    [McpServerTool(Name = "list_charts", ReadOnly = true)]
    [Description(
        "Lists the Sierra Chart charts that have the \"ACSIL MCP Chart Agent\" study, across all open chartbooks. " +
        "Charts without the study are not visible. Use a chart's `id` with the other tools. " +
        "Bar times are chart-local wall-clock times in the chart's `timeZone`. " +
        "`lastUpdateAgeMs` is how long ago the chart's agent last ran: a large value means the chart is hidden " +
        "or Sierra Chart is busy, and requests to that chart will be slow or time out.")]
    public static async Task<string> ListCharts(
        AgentClient client,
        ILogger<ChartTools> logger,
        CancellationToken cancellationToken = default)
    {
        try
        {
            var result = await client.SendAsync("LIST_CHARTS", cancellationToken: cancellationToken).ConfigureAwait(false);
            return McpJson.Serialize(ToListChartsResult(result));
        }
        catch (AgentException ex)
        {
            logger.LogWarning(ex, "list_charts failed");
            return McpJson.Error(ex.Message);
        }
    }

    public static ListChartsResult ToListChartsResult(JsonElement result)
    {
        var charts = result.GetProperty("charts").EnumerateArray()
            .Select(chart => new ChartSummary(
                chart.GetProperty("id").GetString()!,
                chart.GetProperty("chartbook").GetString()!,
                chart.GetProperty("chartNumber").GetInt32(),
                chart.GetProperty("name").GetString()!,
                chart.GetProperty("symbol").GetString()!,
                BarPeriod.Format(
                    chart.GetProperty("chartDataType").GetInt32(),
                    chart.GetProperty("barPeriodType").GetInt32(),
                    chart.GetProperty("barPeriodParam1").GetInt32()),
                chart.GetProperty("timeZone").GetString()!,
                chart.GetProperty("barCount").GetInt32(),
                NullIfEmpty(chart.GetProperty("firstBarTime").GetString()),
                NullIfEmpty(chart.GetProperty("lastBarTime").GetString()),
                chart.GetProperty("lastUpdateAgeMs").GetInt64(),
                chart.GetProperty("pendingRequests").GetInt32()))
            .ToList();
        return new ListChartsResult(charts);
    }

    private static string? NullIfEmpty(string? value) => string.IsNullOrEmpty(value) ? null : value;
}
