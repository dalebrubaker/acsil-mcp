// Framing.h — message framing on the loopback socket (docs/protocol.md).
//
// A frame is a 4-byte little-endian payload length followed by that many bytes of UTF-8 JSON.
// The server's companion is AcsilMcp.Server.Framing; both sides must agree.

#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace acsilmcp
{
    inline constexpr std::uint32_t kMaxFrameBytes = 64u * 1024u * 1024u;
    inline constexpr std::size_t kFrameHeaderBytes = 4;

    inline std::string EncodeFrame(std::string_view payload)
    {
        if (payload.size() > kMaxFrameBytes)
        {
            throw std::length_error("frame payload exceeds kMaxFrameBytes");
        }

        const auto length = static_cast<std::uint32_t>(payload.size());
        std::string frame;
        frame.reserve(kFrameHeaderBytes + payload.size());
        for (int shift = 0; shift < 32; shift += 8)
        {
            frame.push_back(static_cast<char>((length >> shift) & 0xFFu));
        }
        frame.append(payload);
        return frame;
    }

    // Accumulates bytes from a stream socket and yields complete payloads.
    class FrameDecoder
    {
    public:
        void Append(const char* data, std::size_t count)
        {
            buffer_.append(data, count);
        }

        // Returns the next complete payload, or nullopt if more bytes are needed.
        // Throws std::length_error when a header announces more than kMaxFrameBytes.
        std::optional<std::string> Next()
        {
            if (buffer_.size() < kFrameHeaderBytes)
            {
                return std::nullopt;
            }

            std::uint32_t length = 0;
            for (int i = 0; i < 4; ++i)
            {
                length |= static_cast<std::uint32_t>(static_cast<unsigned char>(buffer_[i])) << (8 * i);
            }
            if (length > kMaxFrameBytes)
            {
                throw std::length_error("frame header announces more than kMaxFrameBytes");
            }
            if (buffer_.size() < kFrameHeaderBytes + length)
            {
                return std::nullopt;
            }

            std::string payload = buffer_.substr(kFrameHeaderBytes, length);
            buffer_.erase(0, kFrameHeaderBytes + length);
            return payload;
        }

    private:
        std::string buffer_;
    };
}
