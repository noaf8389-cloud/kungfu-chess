#pragma once

#include <memory>
#include <string>

#include <ixwebsocket/IXWebSocket.h>

#include "bus/event_bus.hpp"
#include "game_history_store.hpp"
#include "game_session.hpp"
#include "nats_event_mirror.hpp"
#include "player_registry.hpp"
#include "network/command_handler.hpp"
#include "network/network_publisher.hpp"
#include "rating/rating_updater.hpp"
#include "logic/model/game_state.hpp"
#include "db/player_account_store.hpp"

class NatsPublisher;

// One independent, self-contained running match: its own board/state, its own
// event bus, its own 2-color PlayerRegistry, and the session/network/rating
// collaborators wired around them. A server can own any number of these
// instead of exactly one global game.
class GameMatch {
public:
    // Builds a fresh match on startingBoard. accounts is shared across every
    // match on the server (one accounts database, not one per match). natsPublisher
    // is likewise shared — this match's GameOverEvent is mirrored through it.
    // Default: owns its own NoOpGameHistoryStore (recorded results are discarded).
    GameMatch(Board startingBoard, PlayerAccountStore& accounts, NatsPublisher& natsPublisher);
    // Injected: this match's result is recorded through historyStore, shared
    // across every match on the server like accounts/natsPublisher.
    GameMatch(Board startingBoard, PlayerAccountStore& accounts, NatsPublisher& natsPublisher, GameHistoryStore& historyStore);

    // Advances this match by deltaMs (moves/jumps/rests) and forfeits any
    // player whose disconnect grace period has expired. Call once per server
    // tick, for every active match.
    void update(long long deltaMs);

    // Routes one raw client command (e.g. a "click"/"jump" JSON string) into
    // this match on behalf of actingColor.
    void handleMessage(const std::string& rawMessage, char actingColor);

    // Registers a socket to receive this match's broadcasts (moves, captures,
    // game-over, ...) and sends it the current board snapshot.
    void addConnection(std::shared_ptr<ix::WebSocket> connection);

    // Unregisters a socket from this match's broadcasts.
    void removeConnection(ix::WebSocket* connection);

    // This match's player/color registry — assign colors, record usernames,
    // and manage disconnect/reconnect for the two players in it.
    PlayerRegistry& players();

    // This match's event bus — subscribe to its game events (e.g. to detect when it ends).
    EventBus& bus();

    // Whether this match has ended (a king was captured or a player forfeited).
    bool isOver() const;

private:
    GameState gameState_;
    EventBus bus_;
    PlayerRegistry players_;
    GameSession session_;
    CommandHandler commandHandler_;
    NetworkPublisher publisher_;
    std::unique_ptr<GameHistoryStore> ownedHistoryStore_;
    GameHistoryStore& historyStore_;
    RatingUpdater ratingUpdater_;
    NatsEventMirror natsEventMirror_;
};
