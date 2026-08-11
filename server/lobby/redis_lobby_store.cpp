#include "redis_lobby_store.hpp"

#include <stdexcept>

#include <hiredis.h>

namespace {
    std::string connectionsKey() { return "lobby:connections"; }
    std::string identityKey(LobbyStore::ConnectionId id) { return "lobby:identity:" + std::to_string(id); }

    // Owns a redisReply so it's freed on every exit path, including exceptions.
    class ReplyGuard {
    public:
        explicit ReplyGuard(void* reply) : reply_(static_cast<redisReply*>(reply)) {}
        ReplyGuard(const ReplyGuard&) = delete;
        ReplyGuard& operator=(const ReplyGuard&) = delete;
        ~ReplyGuard() {
            if (reply_) freeReplyObject(reply_);
        }

        redisReply* operator->() const { return reply_; }
        redisReply* get() const { return reply_; }

    private:
        redisReply* reply_;
    };
}

void RedisLobbyStore::ContextDeleter::operator()(redisContext* ctx) const {
    if (ctx) redisFree(ctx);
}

RedisLobbyStore::RedisLobbyStore(const std::string& host, int port) {
    redisContext* ctx = redisConnect(host.c_str(), port);
    if (ctx == nullptr) {
        throw std::runtime_error("RedisLobbyStore: redisConnect returned null (out of memory)");
    }
    if (ctx->err) {
        std::string message = ctx->errstr;
        redisFree(ctx);
        throw std::runtime_error("RedisLobbyStore: failed to connect to Redis at " + host + ":" +
                                  std::to_string(port) + ": " + message);
    }
    context_.reset(ctx);
}

RedisLobbyStore::~RedisLobbyStore() = default;

LobbyStore::ConnectionId RedisLobbyStore::nextConnectionId() {
    std::lock_guard<std::mutex> lock(mutex_);
    ReplyGuard reply(redisCommand(context_.get(), "INCR lobby:next_id"));
    if (!reply.get() || reply->type != REDIS_REPLY_INTEGER) {
        throw std::runtime_error("RedisLobbyStore::nextConnectionId: unexpected reply from INCR");
    }
    return static_cast<ConnectionId>(reply->integer);
}

void RedisLobbyStore::addConnection(ConnectionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string idStr = std::to_string(id);
    ReplyGuard reply(redisCommand(context_.get(), "SADD %s %s", connectionsKey().c_str(), idStr.c_str()));
}

void RedisLobbyStore::removeConnection(ConnectionId id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string idStr = std::to_string(id);
    ReplyGuard removeMembership(
        redisCommand(context_.get(), "SREM %s %s", connectionsKey().c_str(), idStr.c_str()));
    ReplyGuard removeIdentity(redisCommand(context_.get(), "DEL %s", identityKey(id).c_str()));
}

bool RedisLobbyStore::hasConnection(ConnectionId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string idStr = std::to_string(id);
    ReplyGuard reply(redisCommand(context_.get(), "SISMEMBER %s %s", connectionsKey().c_str(), idStr.c_str()));
    return reply.get() && reply->type == REDIS_REPLY_INTEGER && reply->integer != 0;
}

void RedisLobbyStore::setIdentity(ConnectionId id, const std::string& username, int rating) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = identityKey(id);
    std::string ratingStr = std::to_string(rating);
    ReplyGuard reply(redisCommand(context_.get(), "HSET %s username %s rating %s",
                                   key.c_str(), username.c_str(), ratingStr.c_str()));
}

std::optional<LobbyStore::Identity> RedisLobbyStore::identityFor(ConnectionId id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = identityKey(id);

    ReplyGuard usernameReply(redisCommand(context_.get(), "HGET %s username", key.c_str()));
    if (!usernameReply.get() || usernameReply->type != REDIS_REPLY_STRING) return std::nullopt;

    ReplyGuard ratingReply(redisCommand(context_.get(), "HGET %s rating", key.c_str()));
    if (!ratingReply.get() || ratingReply->type != REDIS_REPLY_STRING) return std::nullopt;

    return Identity{usernameReply->str, std::stoi(ratingReply->str)};
}
