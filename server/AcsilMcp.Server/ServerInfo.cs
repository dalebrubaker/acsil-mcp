namespace AcsilMcp.Server;

public static class ServerInfo
{
    public const string Name = "acsil-mcp";

    public const string Version = "0.0.0";

    /// <summary>
    /// Wire protocol version spoken with the AcsilMcp DLL. Must equal
    /// <c>acsilmcp::kProtocolVersion</c> in native/core/Version.h; bump both together.
    /// </summary>
    public const int ProtocolVersion = 1;
}
