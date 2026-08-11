#include "../../catch2/catch_amalgamated.hpp"

#include "bus/nats_publisher.hpp"

namespace {
    // Port 1 is a privileged port nothing listens on — reliably unreachable
    // without needing a real NATS server for these tests.
    const std::string kUnreachableUrl = "nats://127.0.0.1:1";
}

TEST_CASE("NatsPublisher does not throw when the server is unreachable") {
    REQUIRE_NOTHROW(NatsPublisher(kUnreachableUrl));
}

TEST_CASE("isConnected is false when the server is unreachable") {
    NatsPublisher publisher(kUnreachableUrl);
    REQUIRE(publisher.isConnected() == false);
}

TEST_CASE("publish on a disconnected publisher does not crash") {
    NatsPublisher publisher(kUnreachableUrl);
    REQUIRE_NOTHROW(publisher.publish("test.subject", "payload"));
}
