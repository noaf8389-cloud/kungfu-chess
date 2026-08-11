#include "websocket_server.hpp"

#include <iostream>
#include <utility>
#include <vector>

#include "login_message.hpp"
#include "../matches/game_match.hpp"
#include "../third_party/nlohmann/json.hpp"
#include "logging/logger.hpp"
#include "room_message.hpp"

using json = nlohmann::json;

namespace {
    constexpr long long kDisconnectGraceMs = 20000;
}

GameWebSocketServer::GameWebSocketServer(int port, LobbyRegistry& lobbyRegistry, ShardLookup& gameRegistry, GameAllocator& gameAllocator,
                                  MatchQueue& matchQueue, PlayerAccountStore& accounts, Board templateBoard)
    : lobbyRegistry_(lobbyRegistry), gameRegistry_(gameRegistry),
      matchQueue_(matchQueue), accounts_(accounts), templateBoard_(std::move(templateBoard)),
      authService_(accounts, lobbyRegistry, gameRegistry),
      roomsService_(gameRegistry, gameAllocator, accounts, matchQueue, templateBoard_),
      server_(port, "0.0.0.0") {

    matchQueue_.setOnNoMatchFound([this](const std::string& username) {
        ix::WebSocket* socket = nullptr;
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            auto it = waitingByUsername_.find(username);
            if (it == waitingByUsername_.end()) return;
            socket = it->second;
            waitingByUsername_.erase(it);
        }

        json response;
        response["type"] = "NoMatchFoundEvent";
        socket->send(response.dump());
        Logger::info("No match found for username=" + username);
    });

    server_.setOnConnectionCallback(
        [this](std::weak_ptr<ix::WebSocket> weakWebSocket,
               std::shared_ptr<ix::ConnectionState> /*connectionState*/) {
            auto webSocket = weakWebSocket.lock();
            if (!webSocket) {
                return;
            }

            ix::WebSocket* rawConnection = webSocket.get();
            LobbyRegistry::ConnectionId lobbyId = lobbyRegistry_.registerConnection();
            Logger::info("Client connected (lobbyId=" + std::to_string(lobbyId) + ")");

            {
                std::lock_guard<std::mutex> lock(connectionsMutex_);
                connections_[rawConnection] = std::make_shared<ConnectionState>(
                    ConnectionState{lobbyId, webSocket, std::nullopt, nullptr, 0});
            }

            webSocket->setOnMessageCallback(
                [this, rawConnection](const ix::WebSocketMessagePtr& message) {
                    if (message->type == ix::WebSocketMessageType::Message) {
                        // Only the map lookup happens under the lock; a stable shared_ptr
                        // copy lets the actual handling (DB access, matchmaking, network
                        // sends) run without blocking every other connection's messages.
                        std::shared_ptr<ConnectionState> statePtr;
                        {
                            std::lock_guard<std::mutex> lock(connectionsMutex_);
                            auto it = connections_.find(rawConnection);
                            if (it == connections_.end()) return;
                            statePtr = it->second;
                        }
                        ConnectionState& state = *statePtr;

                        if (auto request = parseLoginRequest(message->str)) {
                            handleLogin(rawConnection, state, *request);
                            return;
                        }

                        if (parseCreateRoomRequest(message->str)) {
                            handleCreateRoom(state);
                            return;
                        }

                        if (auto joinRequest = parseJoinRoomRequest(message->str)) {
                            handleJoinRoom(state, *joinRequest);
                            return;
                        }

                        if (parseFindMatchRequest(message->str)) {
                            handleFindMatch(rawConnection, state);
                            return;
                        }

                        if (state.match != nullptr) {
                            try {
                                handleGameMessage(state, message->str);
                            } catch (const std::exception& e) {
                                Logger::error("Exception while handling message \"" + message->str + "\": " + e.what());
                            }
                        }
                    } else if (message->type == ix::WebSocketMessageType::Close) {
                        std::shared_ptr<ConnectionState> statePtr;
                        {
                            std::lock_guard<std::mutex> lock(connectionsMutex_);
                            auto it = connections_.find(rawConnection);
                            if (it == connections_.end()) return;
                            statePtr = it->second;
                            connections_.erase(it);
                        }
                        handleClose(rawConnection, *statePtr);
                    }
                });
        });
}

void GameWebSocketServer::handleLogin(ix::WebSocket* rawConnection, ConnectionState& state, const LoginRequest& request) {
    AuthResult result = authService_.login(state.lobbyId, request.username, request.password);

    if (result.success) {
        state.username = request.username;
        if (result.existingMatch != nullptr) {
            attachToMatch(state, result.existingMatch, /*isResume=*/true);
            Logger::info("Session resumed: username=" + request.username);
        }
    }

    json response;
    response["type"] = "LoginResultEvent";
    response["payload"] = {{"success", result.success}, {"rating", result.rating}};
    state.socket->send(response.dump());
}

void GameWebSocketServer::handleCreateRoom(ConnectionState& state) {
    if (!state.username.has_value() || state.match != nullptr) return;

    waitingByUsername_.erase(*state.username);

    CreateRoomResult result = roomsService_.createRoom(*state.username);
    attachToMatch(state, result.match, /*isResume=*/false);

    json response;
    response["type"] = "RoomCreatedEvent";
    response["payload"] = {{"roomCode", result.roomCode}};
    state.socket->send(response.dump());
    Logger::info("Room created: username=" + *state.username + " roomCode=" + result.roomCode);
}

void GameWebSocketServer::handleJoinRoom(ConnectionState& state, const JoinRoomRequest& request) {
    if (!state.username.has_value() || state.match != nullptr) return;

    waitingByUsername_.erase(*state.username);

    JoinRoomResult result = roomsService_.joinRoom(*state.username, request.roomCode);
    if (!result.success) {
        json response;
        response["type"] = "JoinRoomFailedEvent";
        state.socket->send(response.dump());
        Logger::info("Join room failed: username=" + *state.username + " roomCode=" + request.roomCode);
        return;
    }
    attachToMatch(state, result.match, /*isResume=*/false);
}

void GameWebSocketServer::handleFindMatch(ix::WebSocket* rawConnection, ConnectionState& state) {
    if (!state.username.has_value() || state.match != nullptr) return;

    int rating = accounts_.ratingFor(*state.username).value_or(1200);
    matchQueue_.enqueue(*state.username, rating);
    waitingByUsername_[*state.username] = rawConnection;
    Logger::info("Enqueued for matchmaking: username=" + *state.username + " rating=" + std::to_string(rating));
}

void GameWebSocketServer::attachToMatch(ConnectionState& state, GameMatch* match, bool isResume) {
    PlayerId matchPlayerId = match->players().registerConnection();
    if (isResume) {
        match->players().tryResumeSession(matchPlayerId, *state.username);
    } else {
        match->players().setUsername(matchPlayerId, *state.username);
    }
    match->addConnection(state.socket);

    state.match = match;
    state.matchPlayerId = matchPlayerId;

    Logger::info(std::string(isResume ? "Player resumed match: username=" : "Player joined new match: username=") + *state.username);
}

void GameWebSocketServer::handleGameMessage(ConnectionState& state, const std::string& rawMessage) {
    std::optional<char> color = state.match->players().colorFor(state.matchPlayerId);
    if (!color.has_value()) return;
    state.match->handleMessage(rawMessage, *color);
}

void GameWebSocketServer::handleClose(ix::WebSocket* rawConnection, ConnectionState& state) {
    if (state.match != nullptr) {
        std::optional<char> color = state.match->players().colorFor(state.matchPlayerId);
        if (color.has_value()) {
            Logger::info("Client disconnected, entering " + std::to_string(kDisconnectGraceMs) +
                         "ms grace (username=" + *state.username + ")");
            state.match->players().beginDisconnectGrace(state.matchPlayerId, kDisconnectGraceMs);
        }
        state.match->removeConnection(rawConnection);
    } else if (state.username.has_value()) {
        matchQueue_.dequeue(*state.username);
        waitingByUsername_.erase(*state.username);
        Logger::info("Client disconnected while waiting for a match (username=" + *state.username + ")");
    } else {
        Logger::info("Client disconnected (lobbyId=" + std::to_string(state.lobbyId) + ")");
    }

    lobbyRegistry_.unregisterConnection(state.lobbyId);
}

void GameWebSocketServer::attachNewlyMatchedConnections() {
    std::vector<std::pair<std::shared_ptr<ConnectionState>, GameMatch*>> toAttach;
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (auto it = waitingByUsername_.begin(); it != waitingByUsername_.end(); ) {
            GameMatch* match = gameRegistry_.matchFor(it->first);
            if (match != nullptr) {
                auto connIt = connections_.find(it->second);
                if (connIt != connections_.end()) {
                    toAttach.emplace_back(connIt->second, match);
                }
                it = waitingByUsername_.erase(it);
            } else {
                ++it;
            }
        }
    }

    // attachToMatch sends the GameStartedEvent snapshot over the network, so it
    // runs outside connectionsMutex_ like every other handler above.
    for (auto& [statePtr, match] : toAttach) {
        attachToMatch(*statePtr, match, /*isResume=*/false);
    }
}

bool GameWebSocketServer::start() {
    auto result = server_.listen();
    if (!result.first) {
        std::cerr << "WebSocket server failed to listen: " << result.second << std::endl;
        Logger::error("WebSocket server failed to listen: " + result.second);
        return false;
    }
    server_.start();
    return true;
}

void GameWebSocketServer::wait() {
    server_.wait();
}
