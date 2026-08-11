#include "game_match.hpp"

#include <utility>

#include "no_op_game_history_store.hpp"

GameMatch::GameMatch(Board startingBoard, PlayerAccountStore& accounts, NatsPublisher& natsPublisher)
    : gameState_(std::move(startingBoard)),
      session_(gameState_, bus_),
      commandHandler_(session_),
      publisher_(bus_, session_),
      ownedHistoryStore_(std::make_unique<NoOpGameHistoryStore>()), historyStore_(*ownedHistoryStore_),
      ratingUpdater_(bus_, players_, accounts, historyStore_),
      natsEventMirror_(bus_, natsPublisher) {
    players_.setOnDisconnect([this](char color, long long graceMs) { session_.playerDisconnected(color, graceMs); });
    players_.setOnReconnect([this](char color) { session_.playerReconnected(color); });
}

GameMatch::GameMatch(Board startingBoard, PlayerAccountStore& accounts, NatsPublisher& natsPublisher, GameHistoryStore& historyStore)
    : gameState_(std::move(startingBoard)),
      session_(gameState_, bus_),
      commandHandler_(session_),
      publisher_(bus_, session_),
      historyStore_(historyStore),
      ratingUpdater_(bus_, players_, accounts, historyStore_),
      natsEventMirror_(bus_, natsPublisher) {
    players_.setOnDisconnect([this](char color, long long graceMs) { session_.playerDisconnected(color, graceMs); });
    players_.setOnReconnect([this](char color) { session_.playerReconnected(color); });
}

void GameMatch::update(long long deltaMs) {
    session_.update(deltaMs);
    for (char color : players_.extractExpiredDisconnects()) {
        session_.forfeit(color);
    }
}

void GameMatch::handleMessage(const std::string& rawMessage, char actingColor) {
    commandHandler_.handleMessage(rawMessage, actingColor);
}

void GameMatch::addConnection(std::shared_ptr<ix::WebSocket> connection) {
    publisher_.addConnection(std::move(connection));
}

void GameMatch::removeConnection(ix::WebSocket* connection) {
    publisher_.removeConnection(connection);
}

PlayerRegistry& GameMatch::players() { return players_; }
EventBus& GameMatch::bus() { return bus_; }
bool GameMatch::isOver() const { return gameState_.isGameOver(); }
