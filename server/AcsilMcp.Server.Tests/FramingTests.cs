using Xunit;

namespace AcsilMcp.Server.Tests;

/// <summary>Companion of native/tests/FramingTests.cpp; both sides must agree.</summary>
public class FramingTests
{
    [Fact]
    public async Task WritesLittleEndianLengthPrefix()
    {
        using var stream = new MemoryStream();
        await Framing.WriteFrameAsync(stream, "abc", CancellationToken.None);
        Assert.Equal(new byte[] { 3, 0, 0, 0, (byte)'a', (byte)'b', (byte)'c' }, stream.ToArray());
    }

    [Fact]
    public async Task RoundTripsUtf8AndEmptyFrames()
    {
        using var stream = new MemoryStream();
        await Framing.WriteFrameAsync(stream, "{\"symbol\":\"€\"}", CancellationToken.None);
        await Framing.WriteFrameAsync(stream, "", CancellationToken.None);
        stream.Position = 0;

        Assert.Equal("{\"symbol\":\"€\"}", await Framing.ReadFrameAsync(stream, CancellationToken.None));
        Assert.Equal("", await Framing.ReadFrameAsync(stream, CancellationToken.None));
        Assert.Null(await Framing.ReadFrameAsync(stream, CancellationToken.None));
    }

    [Fact]
    public async Task RejectsOversizedHeader()
    {
        var header = BitConverter.GetBytes((uint)Framing.MaxFrameBytes + 1);
        using var stream = new MemoryStream(header);
        await Assert.ThrowsAsync<InvalidDataException>(() => Framing.ReadFrameAsync(stream, CancellationToken.None));
    }

    [Fact]
    public async Task RejectsTruncatedHeader()
    {
        using var stream = new MemoryStream([1, 0]);
        await Assert.ThrowsAsync<EndOfStreamException>(() => Framing.ReadFrameAsync(stream, CancellationToken.None));
    }
}
