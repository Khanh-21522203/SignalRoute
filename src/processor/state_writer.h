#pragma once

/**
 * SignalRoute — State Writer
 *
 * Atomically updates device state in Redis and manages H3 cell index.
 * Combines: sequence guard CAS + device hash update + H3 cell migration.
 */

#include "../common/clients/redis_client.h"
#include "../common/spatial/h3_index.h"
#include "../common/types/device_state.h"
#include "../common/types/location_event.h"

namespace signalroute {

struct StateWriteOutcome {
    bool accepted = false;
    DeviceState state;
    int64_t previous_h3_cell = 0;
};

class StateWriter {
public:
    StateWriter(RedisClient& redis, H3Index& h3, int device_ttl_s);

    /**
     * Atomically update device state + H3 cell index.
     *
     * Steps:
     *   1. Encode (lat, lon) → new H3 cell
     *   2. Call Redis CAS update (atomically checks seq + writes state)
     *   3. If accepted AND cell changed:
     *      a. SREM device from old cell set
     *      b. SADD device to new cell set
     *
     * @param event The incoming location event
     * @return true if accepted (seq was newer), false if rejected
     *
     * TODO: Implement
     */
    bool write(const LocationEvent& event);

    /**
     * Write state and return enough context for downstream in-process events.
     */
    StateWriteOutcome write_with_result(const LocationEvent& event);

private:
    RedisClient& redis_;
    H3Index& h3_;
    int device_ttl_s_;
};

} // namespace signalroute
