#include "../../catch2/catch_amalgamated.hpp"
#include "../../../auth/auth_service.hpp"
#include "../../../db/sqlite_player_account_store.hpp"
#include "../../../matches/game_registry.hpp"

TEST_CASE("AuthService::login creates a new account on first login") {
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobby;
    GameRegistry registry;
    AuthService auth(accounts, lobby, registry);

    auto lobbyId = lobby.registerConnection();
    AuthResult result = auth.login(lobbyId, "alice", "pw123");

    REQUIRE(result.success);
    REQUIRE(result.rating == 1200);
    REQUIRE(result.existingMatch == nullptr);
}

TEST_CASE("AuthService::login succeeds on the correct password for an existing account") {
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobby;
    GameRegistry registry;
    AuthService auth(accounts, lobby, registry);

    auth.login(lobby.registerConnection(), "alice", "pw123");   // creates the account
    AuthResult result = auth.login(lobby.registerConnection(), "alice", "pw123");

    REQUIRE(result.success);
    REQUIRE(result.rating == 1200);
}

TEST_CASE("AuthService::login fails on the wrong password for an existing account") {
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobby;
    GameRegistry registry;
    AuthService auth(accounts, lobby, registry);

    auth.login(lobby.registerConnection(), "alice", "correct-password");
    AuthResult result = auth.login(lobby.registerConnection(), "alice", "wrong-password");

    REQUIRE_FALSE(result.success);
    REQUIRE(result.existingMatch == nullptr);
}

TEST_CASE("AuthService::login records the identity in LobbyRegistry only on success") {
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobby;
    GameRegistry registry;
    AuthService auth(accounts, lobby, registry);

    auto goodId = lobby.registerConnection();
    auth.login(goodId, "alice", "pw123");
    REQUIRE(lobby.usernameFor(goodId) == std::optional<std::string>("alice"));
    REQUIRE(lobby.ratingFor(goodId) == std::optional<int>(1200));

    auth.login(goodId, "alice", "pw123");   // pre-create the account for the next attempt
    auto badId = lobby.registerConnection();
    auth.login(badId, "alice", "wrong-password");
    REQUIRE_FALSE(lobby.isIdentified(badId));
}

TEST_CASE("AuthService::login returns the existing match when the username already has one") {
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobby;
    GameRegistry registry;
    AuthService auth(accounts, lobby, registry);

    auth.login(lobby.registerConnection(), "alice", "pw123");   // creates the account

    Board board(4, 4);
    board.setCell(0, 0, "wR");
    registry.createMatch(board, accounts, "alice", "bob");

    AuthResult result = auth.login(lobby.registerConnection(), "alice", "pw123");

    REQUIRE(result.success);
    REQUIRE(result.existingMatch != nullptr);
    REQUIRE(result.existingMatch == registry.matchFor("alice"));
}

TEST_CASE("AuthService::login returns no existing match for a brand-new account") {
    SqlitePlayerAccountStore accounts(":memory:");
    LobbyRegistry lobby;
    GameRegistry registry;
    AuthService auth(accounts, lobby, registry);

    AuthResult result = auth.login(lobby.registerConnection(), "brand-new-user", "pw123");

    REQUIRE(result.success);
    REQUIRE(result.existingMatch == nullptr);
}
