#pragma once

/**
 * SignalRoute — Gateway Service
 *
 * Stateless ingestion gateway that accepts GPS events via gRPC and UDP,
 * validates them, rate-limits per device, and publishes to Kafka.
 *
 * Responsibilities:
 *   - Accept IngestBatch/IngestSingle gRPC calls
 *   - Accept UDP datagrams (compact binary format)
 *   - Validate schema, coordinates, timestamps
 *   - Rate-limit per device
 *   - Stamp server_recv_ms
 *   - Publish validated events to Kafka (partitioned by device_id)
 *
 * Does NOT:
 *   - Maintain per-device state
 *   - Run dedup or sequence guard
 *   - Write to Redis or PostGIS
 */

#include "../common/config/config.h"
#include "../common/types/location_event.h"
#include "../common/types/result.h"
#include <memory>
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

namespace signalroute {

// Forward declarations
class EventBus;
class KafkaProducer;
class RateLimiter;
class Validator;

struct IngestResult {
    int accepted = 0;
    int rejected = 0;
    std::vector<std::string> errors;

    [[nodiscard]] bool ok() const { return rejected == 0; }
};

class GatewayService {
public:
    GatewayService();
    ~GatewayService();

    /**
     * Start the gateway service.
     * Initializes Kafka producer, gRPC server, and UDP listener.
     *
     * TODO: Implement:
     *   1. Create KafkaProducer with config.kafka
     *   2. Create Validator with config.gateway
     *   3. Create RateLimiter with config.gateway.rate_limit_rps_per_device
     *   4. Start gRPC server on config.server.grpc_port
     *   5. Start UDP listener on config.server.udp_port
     */
    void start(const Config& config);
    void start(const Config& config, EventBus& event_bus);

    /**
     * Graceful shutdown.
     *
     * TODO: Implement:
     *   1. Stop accepting new connections
     *   2. Drain in-flight requests
     *   3. Flush Kafka producer
     *   4. Shutdown gRPC server
     *   5. Close UDP socket
     */
    void stop();

    /// Health check.
    bool is_healthy() const;
    bool is_event_driven() const;

    Result<LocationEvent, std::string> ingest_one(LocationEvent event);
    IngestResult ingest_batch(const std::vector<LocationEvent>& batch);

    std::size_t tracked_devices_for_test() const;

private:
    void start_with_bus(const Config& config, EventBus* external_bus);

    std::atomic<bool> running_{false};
    Config config_;
    std::unique_ptr<EventBus> owned_bus_;
    EventBus* event_bus_ = nullptr;
    std::unique_ptr<KafkaProducer> producer_;
    std::unique_ptr<Validator> validator_;
    std::unique_ptr<RateLimiter> rate_limiter_;
};

} // namespace signalroute
