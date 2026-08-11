#include "auth_service.hpp"

#include "logging/logger.hpp"

AuthService::AuthService(PlayerAccountStore& accounts, LobbyRegistry& lobbyRegistry, ShardLookup& gameRegistry)
    : accounts_(accounts), lobbyRegistry_(lobbyRegistry), gameRegistry_(gameRegistry) {}

AuthResult AuthService::login(LobbyRegistry::ConnectionId lobbyId, const std::string& username, const std::string& password) {
    LoginResult result = accounts_.loginOrRegister(username, password);
    Logger::info("Login attempt: username=" + username + " success=" + (result.success ? "true" : "false"));

    if (!result.success) {
        return AuthResult{false, result.rating, nullptr};
    }

    lobbyRegistry_.identify(lobbyId, username, result.rating);
    GameMatch* existingMatch = gameRegistry_.matchFor(username);
    return AuthResult{true, result.rating, existingMatch};
}
