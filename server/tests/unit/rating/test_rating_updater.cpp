#include "../../catch2/catch_amalgamated.hpp"
#include "../../../rating/rating_updater.hpp"
#include "bus/event_bus.hpp"
#include "bus/game_events.hpp"
#include "../../../matches/game_history_store.hpp"
#include "../../../player_registry.hpp"
#include "../../../db/player_account_store.hpp"
#include <unordered_map>

namespace {
class FakeAccountStore : public PlayerAccountStore {
public:
    LoginResult loginOrRegister(const std::string&, const std::string&) override { return {true, 1200}; }
    void updateRating(const std::string& username, int newRating) override { ratings_[username] = newRating; }
    std::optional<int> ratingFor(const std::string& username) const override {
        auto it = ratings_.find(username);
        return it != ratings_.end() ? std::optional<int>(it->second) : std::nullopt;
    }
    std::unordered_map<std::string, int> ratings_{{"alice", 1200}, {"bob", 1200}};
};

class FakeHistoryStore : public GameHistoryStore {
public:
    void recordGame(const std::string& winner, const std::string& loser) override {
        recorded.push_back({winner, loser});
    }
    std::vector<GameHistoryEntry> historyFor(const std::string&, int) const override { return {}; }
    std::vector<std::pair<std::string, std::string>> recorded;
};
}

TEST_CASE("RatingUpdater persists updated ratings for both players after GameOverEvent") {
    EventBus bus;
    PlayerRegistry registry;
    FakeAccountStore accounts;

    PlayerId white = registry.registerConnection();
    PlayerId black = registry.registerConnection();
    registry.setUsername(white, "alice");
    registry.setUsername(black, "bob");

    RatingUpdater updater(bus, registry, accounts);

    bus.publish(GameOverEvent{'w', 1000});

    REQUIRE(accounts.ratingFor("alice").value() > 1200);
    REQUIRE(accounts.ratingFor("bob").value() < 1200);
}

TEST_CASE("RatingUpdater does nothing if a color has no registered username") {
    EventBus bus;
    PlayerRegistry registry;
    FakeAccountStore accounts;
    RatingUpdater updater(bus, registry, accounts);

    REQUIRE_NOTHROW(bus.publish(GameOverEvent{'w', 1000}));
}

TEST_CASE("RatingUpdater does nothing when only the winner has a registered username") {
    EventBus bus;
    PlayerRegistry registry;
    FakeAccountStore accounts;

    PlayerId white = registry.registerConnection();
    registry.registerConnection();   // black connected but never sent a login
    registry.setUsername(white, "alice");

    RatingUpdater updater(bus, registry, accounts);
    bus.publish(GameOverEvent{'w', 1000});

    REQUIRE(accounts.ratingFor("alice").value() == 1200);   // unchanged — needs both sides known
}

TEST_CASE("RatingUpdater records the game result through an injected GameHistoryStore") {
    EventBus bus;
    PlayerRegistry registry;
    FakeAccountStore accounts;
    FakeHistoryStore history;

    PlayerId white = registry.registerConnection();
    PlayerId black = registry.registerConnection();
    registry.setUsername(white, "alice");
    registry.setUsername(black, "bob");

    RatingUpdater updater(bus, registry, accounts, history);
    bus.publish(GameOverEvent{'w', 1000});

    REQUIRE(history.recorded.size() == 1);
    REQUIRE(history.recorded[0].first == "alice");
    REQUIRE(history.recorded[0].second == "bob");
}

TEST_CASE("RatingUpdater does not crash when a registered username has no matching account") {
    EventBus bus;
    PlayerRegistry registry;
    FakeAccountStore accounts;   // only seeds "alice"/"bob"

    PlayerId white = registry.registerConnection();
    PlayerId black = registry.registerConnection();
    registry.setUsername(white, "carol");   // not in the store
    registry.setUsername(black, "bob");

    RatingUpdater updater(bus, registry, accounts);
    REQUIRE_NOTHROW(bus.publish(GameOverEvent{'w', 1000}));
    REQUIRE(accounts.ratingFor("bob").value() == 1200);   // untouched
}
