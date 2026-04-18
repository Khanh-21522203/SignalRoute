#include "trip_handler.h"

namespace signalroute {

TripHandler::TripHandler(PostgresClient& pg) : pg_(pg) {}

std::vector<LocationEvent> TripHandler::handle(
    const std::string& device_id,
    int64_t from_ts, int64_t to_ts,
    int /*sample_interval_s*/, int limit)
{
    // TODO: Add downsampling support
    return pg_.query_trip(device_id, from_ts, to_ts, limit);
}

} // namespace signalroute
