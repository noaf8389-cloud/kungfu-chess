#include <string>

#include "../../catch2/catch_amalgamated.hpp"

#include <nats.h>

#include "bus/nats_publisher.hpp"

namespace {
    // Real, containerized NATS — see docker-compose.yml at the repo root.
    const std::string kNatsUrl = "nats://nats:4222";
}

TEST_CASE("NatsPublisher: a published message is actually received by a subscriber") {
    natsConnection* conn = nullptr;
    REQUIRE(natsConnection_ConnectTo(&conn, kNatsUrl.c_str()) == NATS_OK);

    natsSubscription* sub = nullptr;
    REQUIRE(natsConnection_SubscribeSync(&sub, conn, "test.roundtrip") == NATS_OK);

    NatsPublisher publisher(kNatsUrl);
    REQUIRE(publisher.isConnected());
    publisher.publish("test.roundtrip", "hello-nats");

    natsMsg* msg = nullptr;
    // 5s, not 2s: same reasoning as test_process_split_integration.cpp -- this
    // now runs alongside Postgres/Redis/Loki/Promtail in the same
    // docker-compose stack, and local round-trip latency can spike under load.
    natsStatus status = natsSubscription_NextMsg(&msg, sub, 5000);
    REQUIRE(status == NATS_OK);
    REQUIRE(std::string(natsMsg_GetData(msg), natsMsg_GetDataLength(msg)) == "hello-nats");

    natsMsg_Destroy(msg);
    natsSubscription_Destroy(sub);
    natsConnection_Destroy(conn);
}
