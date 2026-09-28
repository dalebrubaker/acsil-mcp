using System.Buffers.Binary;
using System.Text;

namespace AcsilMcp.Server;

/// <summary>
/// Message framing on the loopback socket (docs/protocol.md): a 4-byte little-endian payload length
/// followed by UTF-8 JSON. Companion of native/core/Framing.h; both sides must agree.
/// </summary>
public static class Framing
{
    public const int MaxFrameBytes = 64 * 1024 * 1024;

    public static async Task WriteFrameAsync(Stream stream, string payload, CancellationToken cancellationToken)
    {
        var bytes = Encoding.UTF8.GetBytes(payload);
        if (bytes.Length > MaxFrameBytes)
        {
            throw new InvalidDataException($"Frame payload of {bytes.Length} bytes exceeds {MaxFrameBytes}.");
        }

        var frame = new byte[4 + bytes.Length];
        BinaryPrimitives.WriteUInt32LittleEndian(frame, (uint)bytes.Length);
        bytes.CopyTo(frame, 4);
        await stream.WriteAsync(frame, cancellationToken).ConfigureAwait(false);
        await stream.FlushAsync(cancellationToken).ConfigureAwait(false);
    }

    /// <summary>Reads one frame, or returns null if the peer closed the connection before a header.</summary>
    public static async Task<string?> ReadFrameAsync(Stream stream, CancellationToken cancellationToken)
    {
        var header = new byte[4];
        var read = await stream.ReadAtLeastAsync(header, 4, throwOnEndOfStream: false, cancellationToken).ConfigureAwait(false);
        if (read == 0)
        {
            return null;
        }
        if (read < 4)
        {
            throw new EndOfStreamException("Connection closed inside a frame header.");
        }

        var length = BinaryPrimitives.ReadUInt32LittleEndian(header);
        if (length > MaxFrameBytes)
        {
            throw new InvalidDataException($"Frame header announces {length} bytes, more than {MaxFrameBytes}.");
        }

        var payload = new byte[length];
        await stream.ReadExactlyAsync(payload, cancellationToken).ConfigureAwait(false);
        return Encoding.UTF8.GetString(payload);
    }
}
