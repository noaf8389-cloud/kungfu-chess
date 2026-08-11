#include "nats_publisher.hpp"

#include <nats.h>

#include "logging/logger.hpp"

void NatsPublisher::ConnectionDeleter::operator()(natsConnection* conn) const {
    if (conn) natsConnection_Destroy(conn);
}

NatsPublisher::NatsPublisher() = default;

NatsPublisher::NatsPublisher(const std::string& natsUrl) {
    natsConnection* conn = nullptr;
    natsStatus status = natsConnection_ConnectTo(&conn, natsUrl.c_str());
    if (status != NATS_OK) {
        Logger::warn("NatsPublisher: could not connect to " + natsUrl +
                     " (" + natsStatus_GetText(status) + ") — publishing will be a no-op");
        return;
    }
    connection_.reset(conn);
}

NatsPublisher::~NatsPublisher() = default;

bool NatsPublisher::isConnected() const {
    return static_cast<bool>(connection_);
}

void NatsPublisher::publish(const std::string& subject, const std::string& payload) {
    if (!static_cast<bool>(connection_)) return;

    natsStatus status = natsConnection_Publish(connection_.get(), subject.c_str(),
                                                payload.data(), static_cast<int>(payload.size()));
    if (status != NATS_OK) {
        Logger::warn("NatsPublisher: publish failed (" + std::string(natsStatus_GetText(status)) + ")");
    }
}
