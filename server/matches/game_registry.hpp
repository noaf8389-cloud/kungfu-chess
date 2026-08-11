#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>

#include "bus/nats_publisher.hpp"
#include "game_history_store.hpp"
#include "game_index_store.hpp"
#include "game_match.hpp"
#include "shard_lookup.hpp"
#include "db/player_account_store.hpp"
#include "logic/model/game_state.hpp"

// Owns every currently-active GameMatch on the server. Creates matches, finds
// which match a connection belongs to, and reaps matches once they end.
// Implements ShardLookup so single-shard callers (still every call site
// today) and the multi-shard ShardRegistry (Phase 6) are interchangeable.
class GameRegistry : public ShardLookup {
public:
    using GameId = std::size_t;
    using RoomCode = std::string;

    // Default: owns its own disconnected NatsPublisher (publishing becomes a
    // no-op) and an in-memory GameIndexStore — today's behavior unchanged;
    // used throughout tests.
    GameRegistry();
    // Injected NatsPublisher; still owns its own in-memory GameIndexStore.
    explicit GameRegistry(NatsPublisher& natsPublisher);
    // Both injected — used to wire a RedisGameIndexStore in once Phase 5
    // actually needs the lookup indices shared across processes. Still owns
    // its own NoOpGameHistoryStore.
    GameRegistry(NatsPublisher& natsPublisher, GameIndexStore& indexStore);
    // All three injected — used by WsGateway (Phase 7) to also record every
    // match's result through a real GameHistoryStore.
    GameRegistry(NatsPublisher& natsPublisher, GameIndexStore& indexStore, GameHistoryStore& historyStore);

    // Creates a new match on startingBoard for exactly these two connections,
    // starts tracking it (including auto-removal once it ends), and returns its id.
    // accounts must outlive the match.
    GameId createMatch(Board, PlayerAccountStore&, const std::string& player1Username, const std::string& player2Username);
    // The match this connection currently belongs to, or nullptr if it isn't in
    // one (never matched, or its match already ended and was pruned).
    GameMatch* matchFor(const std::string& username) override;

    // Advances every tracked match by deltaMs. Call once per server tick.
    void updateAll(long long deltaMs) override;

    // Number of matches currently tracked. Mainly for tests/diagnostics.
    std::size_t matchCount() const;

    // Creates a new room with only its creator known; returns a short shareable
    // code a second player/spectators join by. accounts must outlive the match.
    RoomCode createRoom(Board startingBoard, PlayerAccountStore& accounts, const std::string& creatorUsername);

    // Adds a player or spectator to an existing room by its code (color
    // assignment works exactly like any match: the third+ joiner gets no color,
    // i.e. a read-only spectator). Returns nullptr if the code is unknown.
    GameMatch* joinRoom(const RoomCode& roomCode, const std::string& joinerUsername) override;

private:
    GameId insertNewMatch(Board startingBoard, PlayerAccountStore& accounts);
    void trackUsername(const std::string& username, GameId id);
    void trackRoomCode(const RoomCode& roomCode, GameId id);

    std::unique_ptr<NatsPublisher> ownedNatsPublisher_;
    NatsPublisher& natsPublisher_;

    std::unique_ptr<GameIndexStore> ownedIndexStore_;
    GameIndexStore& indexStore_;

    std::unique_ptr<GameHistoryStore> ownedHistoryStore_;
    GameHistoryStore& historyStore_;

    mutable std::recursive_mutex mutex_;
    std::unordered_map<GameId, std::unique_ptr<GameMatch>> matches_;
    std::vector<GameId> finishedGameIds_;

    // Local-only bookkeeping, never goes through indexStore_: which
    // usernames/roomCode point at each GameId, so a finished match is pruned
    // from indexStore_ with a couple of targeted removals instead of a full
    // scan — cheap against an in-process unordered_map, but would mean
    // hitting a remote Redis instance for every key on every server tick
    // otherwise.
    std::unordered_map<GameId, std::vector<std::string>> usernamesForGame_;
    std::unordered_map<GameId, RoomCode> roomCodeForGame_;
};
