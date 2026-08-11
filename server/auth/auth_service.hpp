#pragma once

#include <string>

#include "../db/player_account_store.hpp"
#include "../lobby/lobby_registry.hpp"
#include "../matches/shard_lookup.hpp"

class GameMatch;

struct AuthResult {
    bool success;      ///< False when the username exists and the password didn't match. True for both a verified existing account and a brand-new one (accounts are created on first login).
    int rating;         ///< The account's current ELO rating. Meaningless (0) when success is false.
    GameMatch* existingMatch = nullptr;   ///< Set only when success is true and this username already has a match in progress (i.e. this login is really a reconnect). Attach the caller's connection to it instead of treating this as a fresh session. Always nullptr on failure or for a first-time login.
};

// login/identity logic: verifies credentials, records identity in the lobby, and checks for a resumable match.
class AuthService {
public:
    /// accounts/lobbyRegistry/gameRegistry must outlive this AuthService; it only stores references to them.
    AuthService(PlayerAccountStore& accounts, LobbyRegistry& lobbyRegistry, ShardLookup& gameRegistry);

    /// Call once per login attempt (existing account or first-time signup — both go through this same call).
    /// lobbyId identifies the connection attempting to log in, so its identity can be recorded even before
    /// any match exists for it. Check AuthResult::success before trusting the rest of the result, and check
    /// AuthResult::existingMatch to know whether to resume a match or start fresh.
    AuthResult login(LobbyRegistry::ConnectionId lobbyId, const std::string& username, const std::string& password);

private:
    PlayerAccountStore& accounts_;
    LobbyRegistry& lobbyRegistry_;
    ShardLookup& gameRegistry_;
};
