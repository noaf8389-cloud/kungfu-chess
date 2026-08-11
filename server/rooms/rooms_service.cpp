#include "rooms_service.hpp"

RoomsService::RoomsService(ShardLookup& gameRegistry, GameAllocator& allocator, PlayerAccountStore& accounts, MatchQueue& matchQueue, Board templateBoard)
    : gameRegistry_(gameRegistry), allocator_(allocator), accounts_(accounts), matchQueue_(matchQueue), templateBoard_(std::move(templateBoard)) {}

CreateRoomResult RoomsService::createRoom(const std::string& username) {
    matchQueue_.dequeue(username);

    GameRegistry& registry = allocator_.allocate();
    GameRegistry::RoomCode code = registry.createRoom(templateBoard_, accounts_, username);
    GameMatch* match = registry.matchFor(username);
    return CreateRoomResult{code, match};
}

JoinRoomResult RoomsService::joinRoom(const std::string& username, const GameRegistry::RoomCode& roomCode) {
    matchQueue_.dequeue(username);

    GameMatch* match = gameRegistry_.joinRoom(roomCode, username);
    return JoinRoomResult{match != nullptr, match};
}
