// VersionTests.cpp — Catch2 tests for core/Version.h.
//
// The server has the companion test AcsilMcp.Server.Tests.ServerInfoTests; both sides must agree.

#include <catch2/catch_test_macros.hpp>

#include "core/Version.h"

TEST_CASE("Protocol version matches the server", "[version]")
{
    STATIC_REQUIRE(acsilmcp::kProtocolVersion == 1);
}
