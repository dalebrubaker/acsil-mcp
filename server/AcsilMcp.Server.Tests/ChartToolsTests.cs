using System.Text.Json;
using System.Text.Json.Nodes;
using AcsilMcp.Server.Tools;
using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace AcsilMcp.Server.Tests;

public class ChartToolsTests
{
    private static JsonObject Chart(string id, string firstBarTime) => new()
    {
        ["id"] = id,
        ["chartbook"] = "Futures.Cht",
        ["chartNumber"] = 3,
        ["name"] = "ESZ26 1 Min #3",
        ["symbol"] = "ESZ26",
        ["timeZone"] = "America/New_York",
        ["chartDataType"] = 2,
        ["barPeriodType"] = 0,
        ["barPeriodParam1"] = 60,
        ["barCount"] = 1200,
        ["firstBarTime"] = firstBarTime,
        ["lastBarTime"] = firstBarTime,
        ["lastUpdateAgeMs"] = 150,
        ["pendingRequests"] = 0
    };

    [Fact]
    public async Task ListChartsMapsTheDllReply()
    {
        await using var agent = new FakeAgent(request =>
            request["cmd"]!.GetValue<string>() == "LIST_CHARTS"
                ? FakeAgent.Ok(new JsonObject { ["charts"] = new JsonArray(Chart("Futures.Cht#3", "2026-09-28T09:30:00"), Chart("Futures.Cht#4", "")) })
                : null);
        await using var client = new AgentClient(new AgentClientOptions { Port = agent.Port }, NullLogger<AgentClient>.Instance);

        var json = await ChartTools.ListCharts(client, NullLogger<ChartTools>.Instance, TestContext.Current.CancellationToken);

        using var doc = JsonDocument.Parse(json);
        var charts = doc.RootElement.GetProperty("charts");
        Assert.Equal(2, charts.GetArrayLength());
        Assert.Equal("Futures.Cht#3", charts[0].GetProperty("id").GetString());
        Assert.Equal("1 Min", charts[0].GetProperty("barPeriod").GetString());
        Assert.Equal("2026-09-28T09:30:00", charts[0].GetProperty("firstBarTime").GetString());
        Assert.False(charts[1].TryGetProperty("firstBarTime", out _)); // empty → omitted
    }

    [Fact]
    public async Task ListChartsReturnsErrorObjectWhenDllIsUnreachable()
    {
        await using var client = new AgentClient(new AgentClientOptions { Port = 1 }, NullLogger<AgentClient>.Instance);

        var json = await ChartTools.ListCharts(client, NullLogger<ChartTools>.Instance, TestContext.Current.CancellationToken);

        using var doc = JsonDocument.Parse(json);
        Assert.Contains("Cannot reach AcsilMcp.dll", doc.RootElement.GetProperty("error").GetString());
    }

    [Theory]
    [InlineData(2, 0, 60, "1 Min")]
    [InlineData(2, 0, 300, "5 Min")]
    [InlineData(2, 0, 3600, "60 Min")]
    [InlineData(2, 0, 30, "30 Sec")]
    [InlineData(2, 0, 86400, "1 Day")]
    [InlineData(2, 1, 5000, "5000 Volume")]
    [InlineData(2, 3, 8, "8 Range")]
    [InlineData(1, 0, 0, "Daily")]
    public void FormatsBarPeriods(int dataType, int periodType, int param1, string expected)
    {
        Assert.Equal(expected, BarPeriod.Format(dataType, periodType, param1));
    }

    [Fact]
    public void ReadsPortFromCommandLineThenEnvironment()
    {
        Assert.Equal(23000, AgentClientOptions.FromCommandLine(["--port", "23000"]).Port);
        Assert.Throws<ArgumentException>(() => AgentClientOptions.FromCommandLine(["--port", "0"]));
    }
}
