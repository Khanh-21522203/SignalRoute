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
#include <memory>
#include <atomic>

namespace signalroute {

// Forward declarations
class KafkaProducer;

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

private:
    std::atomic<bool> running_{false};

    // TODO: Add member variables
    // std::unique_ptr<KafkaProducer> producer_;
    // std::unique_ptr<Validator> validator_;
    // std::unique_ptr<RateLimiter> rate_limiter_;
    // std::unique_ptr<GrpcServer> grpc_server_;
    // std::unique_ptr<UdpServer> udp_server_;
};

} // namespace signalroute
