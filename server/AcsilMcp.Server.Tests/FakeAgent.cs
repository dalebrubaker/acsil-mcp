using System.Net;
using System.Net.Sockets;
using System.Text.Json.Nodes;

namespace AcsilMcp.Server.Tests;

/// <summary>
/// Stands in for AcsilMcp.dll: accepts loopback connections and answers each request frame with
/// <see cref="Handler"/>. PING is answered automatically unless the handler handles it.
/// </summary>
public sealed class FakeAgent : IAsyncDisposable
{
    private readonly TcpListener _listener = new(IPAddress.Loopback, 0);
    private readonly CancellationTokenSource _stop = new();
    private readonly List<TcpClient> _clients = [];
    private readonly Task _acceptLoop;

    public FakeAgent(Func<JsonObject, JsonObject?> handler, int protocolVersion = ServerInfo.ProtocolVersion)
    {
        Handler = handler;
        ProtocolVersion = protocolVersion;
        _listener.Start();
        _acceptLoop = AcceptLoopAsync();
    }

    public Func<JsonObject, JsonObject?> Handler { get; }

    public int ProtocolVersion { get; }

    public int Port => ((IPEndPoint)_listener.LocalEndpoint).Port;

    public int ConnectionCount { get; private set; }

    /// <summary>Drops every open connection, as a DLL reload would.</summary>
    public void DropConnections()
    {
        lock (_clients)
        {
            foreach (var client in _clients)
            {
                client.Dispose();
            }
            _clients.Clear();
        }
    }

    public async ValueTask DisposeAsync()
    {
        await _stop.CancelAsync();
        _listener.Stop();
        DropConnections();
        try
        {
            await _acceptLoop;
        }
        catch (Exception ex) when (ex is OperationCanceledException or ObjectDisposedException or SocketException)
        {
            // Expected when the listener stops.
        }
        _stop.Dispose();
    }

    private async Task AcceptLoopAsync()
    {
        while (!_stop.IsCancellationRequested)
        {
            var client = await _listener.AcceptTcpClientAsync(_stop.Token);
            lock (_clients)
            {
                _clients.Add(client);
            }
            ConnectionCount++;
            _ = ServeAsync(client);
        }
    }

    private async Task ServeAsync(TcpClient client)
    {
        try
        {
            var stream = client.GetStream();
            while (await Framing.ReadFrameAsync(stream, _stop.Token) is { } payload)
            {
                var request = JsonNode.Parse(payload)!.AsObject();
                var reply = Handler(request) ?? DefaultReply(request);
                reply["id"] = request["id"]?.DeepClone();
                await Framing.WriteFrameAsync(stream, reply.ToJsonString(), _stop.Token);
            }
        }
        catch (Exception ex) when (ex is IOException or ObjectDisposedException or OperationCanceledException)
        {
            // Connection dropped.
        }
    }

    private JsonObject DefaultReply(JsonObject request) =>
        request["cmd"]?.GetValue<string>() == "PING"
            ? Ok(new JsonObject { ["protocolVersion"] = ProtocolVersion, ["instanceId"] = "fake", ["agentCount"] = 1 })
            : new JsonObject { ["ok"] = false, ["error"] = "unknown command" };

    public static JsonObject Ok(JsonNode result) => new() { ["ok"] = true, ["result"] = result };
}
