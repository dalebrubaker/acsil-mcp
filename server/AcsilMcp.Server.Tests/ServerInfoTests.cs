using Xunit;

namespace AcsilMcp.Server.Tests;

public class ServerInfoTests
{
    /// <summary>Companion of native/tests/VersionTests.cpp; both sides must agree.</summary>
    [Fact]
    public void ProtocolVersionMatchesNative()
    {
        Assert.Equal(1, ServerInfo.ProtocolVersion);
    }
}
