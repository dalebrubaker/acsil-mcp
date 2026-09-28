using System.Net;
using System.Net.Sockets;
using System.Text.Json.Nodes;
using Microsoft.Extensions.Logging.Abstractions;
using Xunit;

namespace AcsilMcp.Server.Tests;

public class AgentClientTests
{
    private static AgentClient CreateClient(int port, TimeSpan? requestTimeout = null) =>
        new(new AgentClientOptions { Port = port, RequestTimeout = requestTimeout ?? TimeSpan.FromSeconds(5) },
            NullLogger<AgentClient>.Instance);

    [Fact]
    public async Task SendsCommandAndReturnsResult()
    {
        await using var agent = new FakeAgent(request =>
            request["cmd"]!.GetValue<string>() == "LIST_CHARTS"
                ? FakeAgent.Ok(new JsonObject { ["charts"] = new JsonArray() })
                : null);
        await using var client = CreateClient(agent.Port);

        var result = await client.SendAsync("LIST_CHARTS", cancellationToken: TestContext.Current.CancellationToken);

        Assert.Equal(0, result.GetProperty("charts").GetArrayLength());
    }

    [Fact]
    public async Task PassesArgumentsAndSurfacesErrorReplies()
    {
        await using var agent = new FakeAgent(request =>
            request["cmd"]!.GetValue<string>() == "GET_BARS"
                ? new JsonObject { ["ok"] = false, ["error"] = $"no agent on chart {request["chart"]}" }
                : null);
        await using var client = CreateClient(agent.Port);

        var ex = await Assert.ThrowsAsync<AgentException>(() =>
            client.SendAsync("GET_BARS", new JsonObject { ["chart"] = "A.Cht#1" }, TestContext.Current.CancellationToken));

        Assert.Equal("no agent on chart A.Cht#1", ex.Message);
    }

    [Fact]
    public async Task ReportsUnreachableDll()
    {
        var port = GetFreePort();
        await using var client = CreateClient(port);

        var ex = await Assert.ThrowsAsync<AgentException>(() =>
            client.SendAsync("LIST_CHARTS", cancellationToken: TestContext.Current.CancellationToken));

        Assert.Contains($"127.0.0.1:{port}", ex.Message);
        Assert.Contains("ACSIL MCP Chart Agent", ex.Message);
    }

    [Fact]
    public async Task RejectsProtocolMismatch()
    {
        await using var agent = new FakeAgent(_ => null, protocolVersion: ServerInfo.ProtocolVersion + 1);
        await using var client = CreateClient(agent.Port);

        var ex = await Assert.ThrowsAsync<AgentException>(() =>
            client.SendAsync("LIST_CHARTS", cancellationToken: TestContext.Current.CancellationToken));

        Assert.Contains("protocol", ex.Message);
    }

    [Fact]
    public async Task ReconnectsAfterTheDllDropsTheConnection()
    {
        await using var agent = new FakeAgent(request =>
            request["cmd"]!.GetValue<string>() == "LIST_CHARTS"
                ? FakeAgent.Ok(new JsonObject { ["charts"] = new JsonArray() })
                : null);
        await using var client = CreateClient(agent.Port);
        var ct = TestContext.Current.CancellationToken;

        await client.SendAsync("LIST_CHARTS", cancellationToken: ct);
        agent.DropConnections();
        await client.SendAsync("LIST_CHARTS", cancellationToken: ct);

        Assert.Equal(2, agent.ConnectionCount);
    }

    [Fact]
    public async Task TimesOutAndRecovers()
    {
        var stall = true;
        await using var agent = new FakeAgent(request =>
        {
            if (request["cmd"]!.GetValue<string>() != "LIST_CHARTS")
            {
                return null;
            }
            if (stall)
            {
                stall = false;
                Thread.Sleep(500);
            }
            return FakeAgent.Ok(new JsonObject { ["charts"] = new JsonArray() });
        });
        await using var client = CreateClient(agent.Port, TimeSpan.FromMilliseconds(200));
        var ct = TestContext.Current.CancellationToken;

        var ex = await Assert.ThrowsAsync<AgentException>(() => client.SendAsync("LIST_CHARTS", cancellationToken: ct));
        Assert.Contains("did not answer", ex.Message);

        var result = await client.SendAsync("LIST_CHARTS", cancellationToken: ct);
        Assert.Equal(0, result.GetProperty("charts").GetArrayLength());
    }

    private static int GetFreePort()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start();
        var port = ((IPEndPoint)listener.LocalEndpoint).Port;
        listener.Stop();
        return port;
    }
}
