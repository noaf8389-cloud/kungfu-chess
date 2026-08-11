#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <httplib.h>

#include "lobby/matchmaker.hpp"
#include "lobby/matchmaker_internal_server.hpp"
#include "lobby/redis_matchmaker_queue_store.hpp"
#include "logging/logger.hpp"
#include "matches/http_match_creator.hpp"
#include "third_party/nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
    constexpr long long kTickMs = 50;

    std::string getEnvOr(const char* name, const std::string& fallback) {
        const char* value = std::getenv(name);
        return value ? std::string(value) : fallback;
    }
}

// Matchmaker process: the single active instance, per the coordination
// decision in Implementation_Roadmap.md Phase 3/5 (this is exactly what
// unblocks the pairing race condition -- there's genuinely only one). Talks
// to the WS Gateway/shard process over HTTP (HttpMatchCreator) instead of
// creating matches in-process.
int main() {
    // LOG_DIR defaults to the current directory (native, unchanged); Compose
    // points it at a shared volume so Promtail can ship every process's log
    // to Loki -- see Implementation_Roadmap.md Phase 7.
    Logger::init(getEnvOr("LOG_DIR", ".") + "/matchmaker.log");

    constexpr int kInternalPort = 8083;

    std::string wsGatewayHost = getEnvOr("WS_GATEWAY_HOST", "127.0.0.1");
    int wsGatewayInternalPort = std::stoi(getEnvOr("WS_GATEWAY_INTERNAL_PORT", "8082"));
    std::string redisHost = getEnvOr("REDIS_HOST", "127.0.0.1");
    int redisPort = std::stoi(getEnvOr("REDIS_PORT", "6379"));

    HttpMatchCreator matchCreator(wsGatewayHost, wsGatewayInternalPort);
    RedisMatchmakerQueueStore queueStore(redisHost, redisPort);
    Matchmaker matchmaker(matchCreator, queueStore);

    httplib::Client wsGatewayClient(wsGatewayHost, wsGatewayInternalPort);
    matchmaker.setOnNoMatchFound([&wsGatewayClient](const std::string& username) {
        json body = {{"username", username}};
        auto result = wsGatewayClient.Post("/internal/no-match-found", body.dump(), "application/json");
        if (!result) {
            Logger::warn("Matchmaker: failed to notify WS Gateway of no-match-found for username=" + username);
        }
    });

    MatchmakerInternalServer internalServer(kInternalPort, matchmaker);
    if (!internalServer.start()) {
        return 1;
    }

    std::cout << "Matchmaker internal API listening on port " << kInternalPort << std::endl;
    Logger::info("Matchmaker internal API listening on port " + std::to_string(kInternalPort));

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kTickMs));
        matchmaker.tick();
    }
}
