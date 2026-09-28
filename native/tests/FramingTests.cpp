// FramingTests.cpp — Catch2 tests for core/Framing.h.
//
// The server has companion tests in AcsilMcp.Server.Tests.FramingTests; both sides must agree.

#include <catch2/catch_test_macros.hpp>

#include "core/Framing.h"

using namespace acsilmcp;

TEST_CASE("EncodeFrame writes a little-endian length prefix", "[framing]")
{
    const auto frame = EncodeFrame("abc");
    REQUIRE(frame.size() == 7);
    REQUIRE(frame.substr(0, 4) == std::string("\x03\x00\x00\x00", 4));
    REQUIRE(frame.substr(4) == "abc");
}

TEST_CASE("FrameDecoder yields frames split across reads", "[framing]")
{
    const auto bytes = EncodeFrame("{\"cmd\":\"PING\"}") + EncodeFrame("");
    FrameDecoder decoder;

    for (std::size_t i = 0; i < 6; ++i)
    {
        decoder.Append(&bytes[i], 1);
        REQUIRE_FALSE(decoder.Next().has_value());
    }
    decoder.Append(bytes.data() + 6, bytes.size() - 6);

    REQUIRE(decoder.Next() == std::optional<std::string>("{\"cmd\":\"PING\"}"));
    REQUIRE(decoder.Next() == std::optional<std::string>(""));
    REQUIRE_FALSE(decoder.Next().has_value());
}

TEST_CASE("FrameDecoder rejects an oversized header", "[framing]")
{
    FrameDecoder decoder;
    const std::uint32_t tooBig = kMaxFrameBytes + 1;
    const char header[4] = {
        static_cast<char>(tooBig & 0xFF), static_cast<char>((tooBig >> 8) & 0xFF),
        static_cast<char>((tooBig >> 16) & 0xFF), static_cast<char>((tooBig >> 24) & 0xFF)};
    decoder.Append(header, 4);
    REQUIRE_THROWS_AS(decoder.Next(), std::length_error);
}
