#include <string>
#include <unordered_map>

#include "../../catch2/catch_amalgamated.hpp"
#include "../../../matches/game_match.hpp"
#include "bus/nats_publisher.hpp"

#include <nats.h>

namespace {
    // Real, containerized NATS — see docker-compose.yml at the repo root.
    const std::string kNatsUrl = "nats://nats:4222";

    class FakeAccountStore : public PlayerAccountStore {
    public:
        LoginResult loginOrRegister(const std::string&, const std::string&) override { return {}; }
        void updateRating(const std::string&, int) override {}
        std::optional<int> ratingFor(const std::string&) const override { return std::nullopt; }
    };

    Board makeBoardWithCapturableKing() {
        Board board(4, 4);
        board.setCell(0, 0, "wR");
        board.setCell(0, 3, "bK");
        return board;
    }
}

TEST_CASE("GameMatch mirrors a real GameOverEvent to NATS") {
    natsConnection* conn = nullptr;
    REQUIRE(natsConnection_ConnectTo(&conn, kNatsUrl.c_str()) == NATS_OK);

    natsSubscription* sub = nullptr;
    REQUIRE(natsConnection_SubscribeSync(&sub, conn, "game.events.game_over") == NATS_OK);

    NatsPublisher natsPublisher(kNatsUrl);
    REQUIRE(natsPublisher.isConnected());

    FakeAccountStore accounts;
    GameMatch match(makeBoardWithCapturableKing(), accounts, natsPublisher);

    // Rook captures the king: completes the game and fires a real GameOverEvent.
    match.handleMessage(R"({"command":"click","row":0,"col":0})", 'w');
    match.handleMessage(R"({"command":"click","row":0,"col":3})", 'w');
    match.update(100000);

    natsMsg* msg = nullptr;
    natsStatus status = natsSubscription_NextMsg(&msg, sub, 2000);
    REQUIRE(status == NATS_OK);

    std::string payload(natsMsg_GetData(msg), natsMsg_GetDataLength(msg));
    REQUIRE(payload.find("\"type\":\"GameOverEvent\"") != std::string::npos);
    REQUIRE(payload.find("\"winner_color\":\"w\"") != std::string::npos);

    natsMsg_Destroy(msg);
    natsSubscription_Destroy(sub);
    natsConnection_Destroy(conn);
}
