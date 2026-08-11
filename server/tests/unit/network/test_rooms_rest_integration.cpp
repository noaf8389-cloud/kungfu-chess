#include <chrono>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../third_party/nlohmann/json.hpp"

#include <httplib.h>

using json = nlohmann::json;

namespace {
    // Real, separately-running ApiGateway + WsGateway containers -- see
    // docker-compose.yml. test_http_rooms_client.cpp / test_http_server.cpp
    // already prove HttpRoomsClient and HttpServer each work correctly in
    // isolation; this proves the real ApiGateway process (with its actual
    // WS_GATEWAY_HOST/WS_GATEWAY_INTERNAL_PORT wiring from api_gateway_main.cpp)
    // actually reaches the real WsGateway process over the network -- same
    // rigor precedent as test_process_split_integration.cpp for
    // WsGateway<->MatchmakerServer.
    const std::string kApiGatewayHost = "api-gateway";
    constexpr int kApiGatewayPort = 8081;
}

TEST_CASE("POST /rooms on the real ApiGateway reaches the real WsGateway and creates a joinable room") {
    httplib::Client client(kApiGatewayHost, kApiGatewayPort);

    // Unique usernames per run so repeated runs against the same long-lived
    // containers don't collide with a still-resumable room from a previous run.
    long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string creator = "restRoomCreator" + std::to_string(nowMs);
    std::string joiner = "restRoomJoiner" + std::to_string(nowMs);

    json createBody = {{"username", creator}};
    auto createRes = client.Post("/rooms", createBody.dump(), "application/json");

    REQUIRE(createRes != nullptr);
    REQUIRE(createRes->status == 201);
    json createResponseBody = json::parse(createRes->body);
    std::string roomCode = createResponseBody.value("roomCode", "");
    REQUIRE_FALSE(roomCode.empty());

    json joinBody = {{"username", joiner}};
    auto joinRes = client.Post("/rooms/" + roomCode + "/join", joinBody.dump(), "application/json");

    REQUIRE(joinRes != nullptr);
    REQUIRE(joinRes->status == 200);
    json joinResponseBody = json::parse(joinRes->body);
    REQUIRE(joinResponseBody["success"] == true);
}

TEST_CASE("POST /rooms/{roomCode}/join on the real ApiGateway returns 404 for an unknown room") {
    httplib::Client client(kApiGatewayHost, kApiGatewayPort);

    json joinBody = {{"username", "restRoomJoinerNoSuchRoom"}};
    auto joinRes = client.Post("/rooms/NOSUCHROOM/join", joinBody.dump(), "application/json");

    REQUIRE(joinRes != nullptr);
    REQUIRE(joinRes->status == 404);
}
