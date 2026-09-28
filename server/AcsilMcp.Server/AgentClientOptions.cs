namespace AcsilMcp.Server;

public sealed class AgentClientOptions
{
    public const int DefaultPort = 22921;

    /// <summary>Loopback port of AcsilMcp.dll; matches the agent study's Port input.</summary>
    public int Port { get; init; } = DefaultPort;

    public TimeSpan ConnectTimeout { get; init; } = TimeSpan.FromSeconds(2);

    /// <summary>Upper bound for one request; the DLL's own per-chart timeout (default 5 s) fires first.</summary>
    public TimeSpan RequestTimeout { get; init; } = TimeSpan.FromSeconds(30);

    /// <summary>Reads the port from <c>--port N</c>, then the ACSIL_MCP_PORT environment variable.</summary>
    public static AgentClientOptions FromCommandLine(string[] args)
    {
        var index = Array.IndexOf(args, "--port");
        var text = index >= 0 && index + 1 < args.Length
            ? args[index + 1]
            : Environment.GetEnvironmentVariable("ACSIL_MCP_PORT");

        if (string.IsNullOrWhiteSpace(text))
        {
            return new AgentClientOptions();
        }
        if (!int.TryParse(text, out var port) || port is < 1 or > 65535)
        {
            throw new ArgumentException($"Invalid port '{text}'; expected 1-65535.");
        }
        return new AgentClientOptions { Port = port };
    }
}
