#include <chrono>
#include <thread>
#include <unordered_map>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../network/http_server.hpp"
#include "../../../db/sqlite_player_account_store.hpp"
#include "../../../matches/game_history_store.hpp"
#include "../../../matches/no_op_game_history_store.hpp"
#include "../../../rooms/rooms_client.hpp"
#include "../../../third_party/nlohmann/json.hpp"

#include <httplib.h>

using json = nlohmann::json;

namespace {
    // Each TEST_CASE uses its own port (like test_websocket_integration.cpp) to
    // avoid TIME_WAIT/port-reuse flakiness between sequential test cases.
    void waitForServerReady() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    class FakeHistoryStore : public GameHistoryStore {
    public:
        void recordGame(const std::string& winner, const std::string& loser) override {
            entries[winner].push_back(GameHistoryEntry{loser, "win", 1000});
            entries[loser].push_back(GameHistoryEntry{winner, "loss", 1000});
        }
        std::vector<GameHistoryEntry> historyFor(const std::string& username, int) const override {
            auto it = entries.find(username);
            return it != entries.end() ? it->second : std::vector<GameHistoryEntry>{};
        }
        std::unordered_map<std::string, std::vector<GameHistoryEntry>> entries;
    };

    class FakeRoomsClient : public RoomsClient {
    public:
        std::optional<std::string> createRoom(const std::string&) override { return roomCodeToReturn; }
        bool joinRoom(const std::string&, const std::string& roomCode) override { return roomCode == joinableRoomCode; }

        std::optional<std::string> roomCodeToReturn = "ABC123";
        std::string joinableRoomCode = "ABC123";
    };
}

TEST_CASE("GET /health returns 200") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18081, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18081);
    auto res = client.Get("/health");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
}

TEST_CASE("POST /login creates a new account and returns the default rating") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18082, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18082);
    auto res = client.Post("/login", R"({"username":"alice","password":"pw123"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json body = json::parse(res->body);
    REQUIRE(body["success"] == true);
    REQUIRE(body["rating"] == 1200);
}

TEST_CASE("POST /login with the wrong password for an existing account fails") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18083, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18083);
    client.Post("/login", R"({"username":"bob","password":"correct"})", "application/json");
    auto res = client.Post("/login", R"({"username":"bob","password":"wrong"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json body = json::parse(res->body);
    REQUIRE(body["success"] == false);
}

TEST_CASE("POST /login with a malformed body returns 400") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18084, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18084);
    auto res = client.Post("/login", "not json", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /login with an empty username returns 400") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18085, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18085);
    auto res = client.Post("/login", R"({"username":"","password":"pw123"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("GET /players/{username}/rating returns the rating for a known account") {
    SqlitePlayerAccountStore accounts(":memory:");
    accounts.loginOrRegister("carol", "pw123");
    HttpServer server(18086, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18086);
    auto res = client.Get("/players/carol/rating");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json body = json::parse(res->body);
    REQUIRE(body["rating"] == 1200);
}

TEST_CASE("GET /players/{username}/rating returns 404 for an unknown account") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18087, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18087);
    auto res = client.Get("/players/ghost/rating");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 404);
}

TEST_CASE("GET /players/{username}/history returns an empty array by default (no GameHistoryStore injected)") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18101, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18101);
    auto res = client.Get("/players/alice/history");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json body = json::parse(res->body);
    REQUIRE(body.is_array());
    REQUIRE(body.empty());
}

TEST_CASE("GET /players/{username}/history returns recorded games from an injected GameHistoryStore") {
    SqlitePlayerAccountStore accounts(":memory:");
    FakeHistoryStore history;
    history.recordGame("alice", "bob");
    HttpServer server(18102, accounts, history);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18102);
    auto res = client.Get("/players/alice/history");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json body = json::parse(res->body);
    REQUIRE(body.size() == 1);
    REQUIRE(body[0]["opponent"] == "bob");
    REQUIRE(body[0]["result"] == "win");
}

TEST_CASE("POST /rooms returns 503 by default (no RoomsClient injected)") {
    SqlitePlayerAccountStore accounts(":memory:");
    HttpServer server(18120, accounts);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18120);
    auto res = client.Post("/rooms", R"({"username":"alice"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 503);
}

TEST_CASE("POST /rooms creates a room via an injected RoomsClient") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18121, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18121);
    auto res = client.Post("/rooms", R"({"username":"alice"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 201);
    json body = json::parse(res->body);
    REQUIRE(body["roomCode"] == "ABC123");
}

TEST_CASE("POST /rooms with an empty username returns 400") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18122, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18122);
    auto res = client.Post("/rooms", R"({"username":""})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /rooms/{roomCode}/join returns 200 on success via an injected RoomsClient") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18123, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18123);
    auto res = client.Post("/rooms/ABC123/join", R"({"username":"bob"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 200);
    json body = json::parse(res->body);
    REQUIRE(body["success"] == true);
}

TEST_CASE("POST /rooms/{roomCode}/join returns 404 for an unknown roomCode") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18124, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18124);
    auto res = client.Post("/rooms/NOSUCH/join", R"({"username":"bob"})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 404);
}

TEST_CASE("POST /rooms/{roomCode}/join with an empty username returns 400") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18125, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18125);
    auto res = client.Post("/rooms/ABC123/join", R"({"username":""})", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /rooms with a malformed body returns 400") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18126, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18126);
    auto res = client.Post("/rooms", "not json", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}

TEST_CASE("POST /rooms/{roomCode}/join with a malformed body returns 400") {
    SqlitePlayerAccountStore accounts(":memory:");
    NoOpGameHistoryStore history;
    FakeRoomsClient roomsClient;
    HttpServer server(18127, accounts, history, roomsClient);
    REQUIRE(server.start());
    waitForServerReady();

    httplib::Client client("127.0.0.1", 18127);
    auto res = client.Post("/rooms/ABC123/join", "not json", "application/json");

    REQUIRE(res != nullptr);
    REQUIRE(res->status == 400);
}
