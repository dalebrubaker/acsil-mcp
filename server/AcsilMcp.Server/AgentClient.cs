using System.Net;
using System.Net.Sockets;
using System.Text.Json;
using System.Text.Json.Nodes;
using Microsoft.Extensions.Logging;

namespace AcsilMcp.Server;

/// <summary>A request the DLL answered with <c>ok: false</c>, or a DLL that cannot be reached.</summary>
public sealed class AgentException(string message) : Exception(message);

/// <summary>
/// Request/reply client for AcsilMcp.dll (docs/protocol.md). One request at a time over one
/// connection; connects lazily, checks the protocol version with PING, and reconnects once when the
/// connection breaks (for example after Sierra Chart reloads the DLL).
/// </summary>
public sealed class AgentClient(AgentClientOptions options, ILogger<AgentClient> logger) : IAsyncDisposable
{
    private readonly SemaphoreSlim _gate = new(1, 1);
    private TcpClient? _tcp;
    private NetworkStream? _stream;
    private long _nextId;

    /// <summary>Sends <paramref name="cmd"/> and returns the reply's <c>result</c>.</summary>
    public async Task<JsonElement> SendAsync(string cmd, JsonObject? arguments = null, CancellationToken cancellationToken = default)
    {
        await _gate.WaitAsync(cancellationToken).ConfigureAwait(false);
        try
        {
            try
            {
                return await SendOnceAsync(cmd, arguments, cancellationToken).ConfigureAwait(false);
            }
            catch (Exception ex) when (ex is IOException or SocketException or ObjectDisposedException)
            {
                logger.LogWarning(ex, "Connection to AcsilMcp.dll broke; reconnecting once");
                ResetConnection();
                return await SendOnceAsync(cmd, arguments, cancellationToken).ConfigureAwait(false);
            }
        }
        finally
        {
            _gate.Release();
        }
    }

    public ValueTask DisposeAsync()
    {
        ResetConnection();
        _gate.Dispose();
        return ValueTask.CompletedTask;
    }

    private async Task<JsonElement> SendOnceAsync(string cmd, JsonObject? arguments, CancellationToken cancellationToken)
    {
        var stream = await EnsureConnectedAsync(cancellationToken).ConfigureAwait(false);
        return await ExchangeAsync(stream, cmd, arguments, cancellationToken).ConfigureAwait(false);
    }

    private async Task<NetworkStream> EnsureConnectedAsync(CancellationToken cancellationToken)
    {
        if (_stream is not null)
        {
            return _stream;
        }

        var tcp = new TcpClient(AddressFamily.InterNetwork) { NoDelay = true };
        using (var connectTimeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken))
        {
            connectTimeout.CancelAfter(options.ConnectTimeout);
            try
            {
                await tcp.ConnectAsync(IPAddress.Loopback, options.Port, connectTimeout.Token).ConfigureAwait(false);
            }
            catch (Exception ex) when (ex is SocketException or OperationCanceledException && !cancellationToken.IsCancellationRequested)
            {
                logger.LogWarning(ex, "Cannot connect to AcsilMcp.dll on 127.0.0.1:{Port}", options.Port);
                tcp.Dispose();
                throw new AgentException(
                    $"Cannot reach AcsilMcp.dll on 127.0.0.1:{options.Port}. Start Sierra Chart and add the " +
                    "\"ACSIL MCP Chart Agent\" study to at least one chart (its Port input must match).");
            }
        }

        var stream = tcp.GetStream();
        try
        {
            var ping = await ExchangeAsync(stream, "PING", null, cancellationToken).ConfigureAwait(false);
            var version = ping.GetProperty("protocolVersion").GetInt32();
            if (version != ServerInfo.ProtocolVersion)
            {
                throw new AgentException(
                    $"AcsilMcp.dll speaks protocol {version} but this server speaks {ServerInfo.ProtocolVersion}. " +
                    "Install matching versions of the DLL and the server.");
            }
            logger.LogInformation("Connected to AcsilMcp.dll instance {InstanceId} on port {Port}",
                ping.GetProperty("instanceId").GetString(), options.Port);
        }
        catch
        {
            tcp.Dispose();
            throw;
        }

        _tcp = tcp;
        _stream = stream;
        return stream;
    }

    private async Task<JsonElement> ExchangeAsync(NetworkStream stream, string cmd, JsonObject? arguments, CancellationToken cancellationToken)
    {
        var id = Interlocked.Increment(ref _nextId);
        var request = arguments?.DeepClone().AsObject() ?? new JsonObject();
        request["id"] = id;
        request["cmd"] = cmd;

        using var timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(options.RequestTimeout);
        string? payload;
        try
        {
            await Framing.WriteFrameAsync(stream, request.ToJsonString(), timeout.Token).ConfigureAwait(false);
            payload = await Framing.ReadFrameAsync(stream, timeout.Token).ConfigureAwait(false);
        }
        catch (OperationCanceledException) when (!cancellationToken.IsCancellationRequested)
        {
            // The stream may hold a late reply; never reuse it.
            ResetConnection();
            throw new AgentException($"AcsilMcp.dll did not answer {cmd} within {options.RequestTimeout.TotalSeconds:0} s.");
        }

        if (payload is null)
        {
            throw new IOException("AcsilMcp.dll closed the connection.");
        }

        using var reply = JsonDocument.Parse(payload);
        var root = reply.RootElement;
        if (!root.TryGetProperty("id", out var replyId) || replyId.GetInt64() != id)
        {
            ResetConnection();
            throw new AgentException($"AcsilMcp.dll answered {cmd} with a mismatched id.");
        }
        if (!root.GetProperty("ok").GetBoolean())
        {
            throw new AgentException(root.GetProperty("error").GetString() ?? "unknown error");
        }
        return root.GetProperty("result").Clone();
    }

    private void ResetConnection()
    {
        _stream?.Dispose();
        _tcp?.Dispose();
        _stream = null;
        _tcp = null;
    }
}
