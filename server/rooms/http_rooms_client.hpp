#pragma once

#include <memory>
#include <string>

#include "rooms_client.hpp"

namespace httplib { class Client; }

// Calls out to a remote WS Gateway/shard process's internal HTTP API
// (ShardInternalServer) to actually create/join the room -- RoomsService
// (and the live GameMatch) lives there, not in the ApiGateway process. Same
// shape as HttpMatchCreator/HttpMatchQueue for Matchmaker<->WsGateway.
class HttpRoomsClient : public RoomsClient {
public:
    HttpRoomsClient(const std::string& host, int port);
    ~HttpRoomsClient();

    std::optional<std::string> createRoom(const std::string& username) override;
    bool joinRoom(const std::string& username, const std::string& roomCode) override;

private:
    std::unique_ptr<httplib::Client> client_;
};
