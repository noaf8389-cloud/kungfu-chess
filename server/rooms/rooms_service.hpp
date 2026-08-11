#pragma once

#include <string>

#include "../db/player_account_store.hpp"
#include "../lobby/match_queue.hpp"
#include "../matches/game_allocator.hpp"
#include "../matches/game_registry.hpp"
#include "../matches/shard_lookup.hpp"
#include "logic/model/game_state.hpp"

class GameMatch;

struct CreateRoomResult {
    GameRegistry::RoomCode roomCode;
    GameMatch* match;   ///< The caller's own match, ready to attach to. Never nullptr — GameRegistry::createRoom always succeeds for a valid caller.
};

struct JoinRoomResult {
    bool success;        ///< False when roomCode is unknown.
    GameMatch* match;    ///< Set only when success is true.
};

// Room-creation/joining logic: cancels any pending matchmaking search for the
// player, then delegates to GameRegistry. No knowledge of sockets or connections.
class RoomsService {
public:
    /// gameRegistry/allocator/accounts/matchQueue must outlive this RoomsService; it only stores references to them.
    RoomsService(ShardLookup& gameRegistry, GameAllocator& allocator, PlayerAccountStore& accounts, MatchQueue& matchQueue, Board templateBoard);

    /// Call when a logged-in, not-yet-matched player requests a new room.
    /// Also cancels any pending matchmaking search for username.
    CreateRoomResult createRoom(const std::string& username);

    /// Call when a logged-in, not-yet-matched player tries to join roomCode.
    /// Also cancels any pending matchmaking search for username.
    /// Check JoinRoomResult::success before using match.
    JoinRoomResult joinRoom(const std::string& username, const GameRegistry::RoomCode& roomCode);

private:
    ShardLookup& gameRegistry_;    // still needed directly for joinRoom (lookup, not allocation)
    GameAllocator& allocator_;     // used only for createRoom (allocation of a brand-new match)
    PlayerAccountStore& accounts_;
    MatchQueue& matchQueue_;
    Board templateBoard_;
};
