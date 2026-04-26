#pragma once

/**
 * SignalRoute — Fence Registry
 *
 * In-memory store of active geofence rules. Loaded from PostGIS at startup,
 * with periodic hot-reload for fence CRUD operations.
 *
 * Thread safety: uses shared_mutex for read-heavy access (many evaluations,
 * rare reloads).
 *
 * Indexed by H3 cell for O(1) candidate lookup: given a device's H3 cell,
 * return all fences whose polyfill contains that cell.
 */

#include "../common/types/geofence_types.h"
#include "../common/clients/postgres_client.h"
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <optional>

namespace signalroute {

class FenceRegistry {
public:
    /// Load all active fences from PostGIS.
    void load(PostgresClient& pg);

    /**
     * Hot-reload: diff current fences with PostGIS and apply changes.
     * Called periodically (every reload_interval_s).
     */
    void reload(PostgresClient& pg);

    /**
     * Get candidate fences whose H3 polyfill contains the given cell.
     *
     * @param h3_cell The device's current H3 cell
     * @return Pointers to matching GeofenceRules (valid until next reload)
     *
     * Thread-safe: acquires shared lock.
     */
    std::vector<const GeofenceRule*> get_candidates(int64_t h3_cell) const;

    /// Total number of loaded fences.
    size_t fence_count() const;

    /// Get one fence by id. Returns a copy to keep reload safety simple.
    std::optional<GeofenceRule> get_fence(const std::string& fence_id) const;

private:
    mutable std::shared_mutex mu_;

    /// All active fences, keyed by fence_id.
    std::unordered_map<std::string, GeofenceRule> fences_;

    /// Inverted index: H3 cell → set of fence_ids whose polyfill contains this cell.
    std::unordered_map<int64_t, std::vector<std::string>> cell_to_fences_;

    /// Rebuild the cell_to_fences_ index from fences_.
    void rebuild_index();
};

} // namespace signalroute
