// SocketServerTests.cpp — Catch2 tests for core/SocketServer.h over a real loopback socket.

#include <catch2/catch_test_macros.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>

#include "core/Framing.h"
#include "core/SocketServer.h"

using namespace acsilmcp;

namespace
{
    struct WinsockScope
    {
        WinsockScope() { WSADATA wsa{}; WSAStartup(MAKEWORD(2, 2), &wsa); }
        ~WinsockScope() { WSACleanup(); }
    };

    SOCKET Connect(std::uint16_t port)
    {
        const SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);
        REQUIRE(connect(s, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0);
        return s;
    }

    std::string RoundTrip(SOCKET s, const std::string& payload)
    {
        const auto frame = EncodeFrame(payload);
        REQUIRE(send(s, frame.data(), static_cast<int>(frame.size()), 0) == static_cast<int>(frame.size()));

        FrameDecoder decoder;
        char buffer[4096];
        while (true)
        {
            if (auto reply = decoder.Next())
            {
                return *reply;
            }
            const int n = recv(s, buffer, sizeof(buffer), 0);
            REQUIRE(n > 0);
            decoder.Append(buffer, static_cast<std::size_t>(n));
        }
    }
}

TEST_CASE("Server answers frames from several clients and stops cleanly", "[socket]")
{
    WinsockScope winsock;
    SocketServer server;
    std::string error;
    REQUIRE(server.Start(0, [](const std::string& p) { return "echo:" + p; }, [](const std::string&) {}, error));
    REQUIRE(server.Port() != 0);

    const SOCKET a = Connect(server.Port());
    const SOCKET b = Connect(server.Port());
    REQUIRE(RoundTrip(a, "one") == "echo:one");
    REQUIRE(RoundTrip(b, "two") == "echo:two");
    REQUIRE(RoundTrip(a, "three") == "echo:three");

    server.Stop(); // must not hang with clients still connected
    REQUIRE_FALSE(server.IsRunning());

    char byte;
    REQUIRE(recv(a, &byte, 1, 0) <= 0);
    closesocket(a);
    closesocket(b);
}

TEST_CASE("A second server cannot take a port in use", "[socket]")
{
    SocketServer first;
    SocketServer second;
    std::string error;
    REQUIRE(first.Start(0, [](const std::string& p) { return p; }, [](const std::string&) {}, error));

    REQUIRE_FALSE(second.Start(first.Port(), [](const std::string& p) { return p; }, [](const std::string&) {}, error));
    REQUIRE(error.find("in use") != std::string::npos);
}

TEST_CASE("Server can restart after Stop", "[socket]")
{
    WinsockScope winsock;
    SocketServer server;
    std::string error;
    REQUIRE(server.Start(0, [](const std::string& p) { return p; }, [](const std::string&) {}, error));
    server.Stop();
    REQUIRE(server.Start(0, [](const std::string& p) { return "again:" + p; }, [](const std::string&) {}, error));

    const SOCKET s = Connect(server.Port());
    REQUIRE(RoundTrip(s, "x") == "again:x");
    closesocket(s);
    server.Stop();
}
