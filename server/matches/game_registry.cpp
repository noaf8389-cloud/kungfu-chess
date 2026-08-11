#include "game_registry.hpp"

#include <random>

#include "bus/game_events.hpp"
#include "in_memory_game_index_store.hpp"
#include "no_op_game_history_store.hpp"

namespace {
    std::string generateRoomCode() {
        static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<std::size_t> pick(0, sizeof(alphabet) - 2);

        std::string code(6, ' ');
        for (char& c : code) c = alphabet[pick(rng)];
        return code;
    }
}

GameRegistry::GameRegistry()
    : ownedNatsPublisher_(std::make_unique<NatsPublisher>()), natsPublisher_(*ownedNatsPublisher_),
      ownedIndexStore_(std::make_unique<InMemoryGameIndexStore>()), indexStore_(*ownedIndexStore_),
      ownedHistoryStore_(std::make_unique<NoOpGameHistoryStore>()), historyStore_(*ownedHistoryStore_) {}

GameRegistry::GameRegistry(NatsPublisher& natsPublisher)
    : natsPublisher_(natsPublisher),
      ownedIndexStore_(std::make_unique<InMemoryGameIndexStore>()), indexStore_(*ownedIndexStore_),
      ownedHistoryStore_(std::make_unique<NoOpGameHistoryStore>()), historyStore_(*ownedHistoryStore_) {}

GameRegistry::GameRegistry(NatsPublisher& natsPublisher, GameIndexStore& indexStore)
    : natsPublisher_(natsPublisher), indexStore_(indexStore),
      ownedHistoryStore_(std::make_unique<NoOpGameHistoryStore>()), historyStore_(*ownedHistoryStore_) {}

GameRegistry::GameRegistry(NatsPublisher& natsPublisher, GameIndexStore& indexStore, GameHistoryStore& historyStore)
    : natsPublisher_(natsPublisher), indexStore_(indexStore), historyStore_(historyStore) {}

GameRegistry::GameId GameRegistry::insertNewMatch(Board startingBoard, PlayerAccountStore& accounts) {
    GameId id = indexStore_.nextGameId();
    auto match = std::make_unique<GameMatch>(std::move(startingBoard), accounts, natsPublisher_, historyStore_);

    match->bus().subscribe<GameOverEvent>([this, id](const GameOverEvent&) {
        std::lock_guard<std::recursive_mutex> innerLock(mutex_);
        finishedGameIds_.push_back(id);
    });

    matches_[id] = std::move(match);
    return id;
}

void GameRegistry::trackUsername(const std::string& username, GameId id) {
    indexStore_.setUsernameGame(username, id);
    usernamesForGame_[id].push_back(username);
}

void GameRegistry::trackRoomCode(const RoomCode& roomCode, GameId id) {
    indexStore_.setRoomCodeGame(roomCode, id);
    roomCodeForGame_[id] = roomCode;
}

GameRegistry::GameId GameRegistry::createMatch(Board startingBoard, PlayerAccountStore& accounts,
                                                const std::string& player1Username, const std::string& player2Username) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GameId id = insertNewMatch(std::move(startingBoard), accounts);
    trackUsername(player1Username, id);
    trackUsername(player2Username, id);
    return id;
}

GameRegistry::RoomCode GameRegistry::createRoom(Board startingBoard, PlayerAccountStore& accounts, const std::string& creatorUsername) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    GameId id = insertNewMatch(std::move(startingBoard), accounts);
    trackUsername(creatorUsername, id);

    RoomCode code;
    // Check-then-set, not atomic -- safe today only because GameRegistry
    // runs in a single process. Once multiple processes share a
    // Redis-backed GameIndexStore (Phase 5), two processes could both see
    // an empty code and both claim it. Needs to become an atomic
    // check-and-claim then (e.g. Redis SETNX or a Lua script), not a
    // separate check + set.
    do {
        code = generateRoomCode();
    } while (indexStore_.gameForRoomCode(code).has_value());
    trackRoomCode(code, id);
    return code;
}

GameMatch* GameRegistry::joinRoom(const RoomCode& roomCode, const std::string& joinerUsername) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto gameId = indexStore_.gameForRoomCode(roomCode);
    if (!gameId.has_value()) return nullptr;

    auto matchIt = matches_.find(*gameId);
    if (matchIt == matches_.end()) return nullptr;

    trackUsername(joinerUsername, *gameId);
    return matchIt->second.get();
}

GameMatch* GameRegistry::matchFor(const std::string& username) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto gameId = indexStore_.gameForUsername(username);
    if (!gameId.has_value()) return nullptr;

    auto matchIt = matches_.find(*gameId);
    return matchIt != matches_.end() ? matchIt->second.get() : nullptr;
}

void GameRegistry::updateAll(long long deltaMs) {
    std::vector<GameMatch*> active;
    {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        active.reserve(matches_.size());
        for (auto& [id, match] : matches_) {
            active.push_back(match.get());
        }
    }

    // Runs outside mutex_: each match's update() broadcasts over the network to
    // its own connections, which shouldn't block unrelated registry lookups
    // (matchFor/createMatch/createRoom/joinRoom) for the whole server tick.
    for (GameMatch* match : active) {
        match->update(deltaMs);
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (GameId id : finishedGameIds_) {
        matches_.erase(id);

        auto usernamesIt = usernamesForGame_.find(id);
        if (usernamesIt != usernamesForGame_.end()) {
            for (const std::string& username : usernamesIt->second) {
                // Only remove if the index still points at this (finished)
                // game -- guards against a rare reorder where the username
                // was already reassigned to a newer game before this
                // cleanup ran.
                auto current = indexStore_.gameForUsername(username);
                if (current.has_value() && *current == id) {
                    indexStore_.removeUsernameGame(username);
                }
            }
            usernamesForGame_.erase(usernamesIt);
        }

        auto roomCodeIt = roomCodeForGame_.find(id);
        if (roomCodeIt != roomCodeForGame_.end()) {
            auto current = indexStore_.gameForRoomCode(roomCodeIt->second);
            if (current.has_value() && *current == id) {
                indexStore_.removeRoomCodeGame(roomCodeIt->second);
            }
            roomCodeForGame_.erase(roomCodeIt);
        }
    }
    finishedGameIds_.clear();
}

std::size_t GameRegistry::matchCount() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return matches_.size();
}
