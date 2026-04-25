#pragma once

/**
 * SignalRoute — Deduplication Window
 *
 * LRU cache keyed by (device_id, seq) for detecting duplicate events.
 * Events are duplicated when Kafka delivers at-least-once.
 *
 * Implementation: LRU eviction policy with TTL expiry.
 * Max entries bounded to control memory usage.
 */

#include <string>
#include <cstdint>
#include <chrono>
#include <list>
#include <unordered_map>

namespace signalroute {

class DedupWindow {
public:
    /**
     * @param max_entries Maximum number of entries before LRU eviction
     * @param ttl_seconds Time-to-live for each entry (seconds)
     */
    DedupWindow(size_t max_entries, int ttl_seconds);

    /**
     * Check if this (device_id, seq) has been seen before.
     *
     * TODO: Implement using std::unordered_map + doubly-linked list (LRU)
     *       Key = hash(device_id, seq)
     *       Value = insertion timestamp (for TTL expiry)
     *
     * Thread safety: called from a single Kafka partition consumer thread,
     *                so no synchronization needed.
     */
    bool is_duplicate(const std::string& device_id, uint64_t seq);

    /**
     * Mark (device_id, seq) as seen.
     * If cache is full, evict the least recently used entry.
     */
    void mark_seen(const std::string& device_id, uint64_t seq);

    /// Current number of entries in the cache.
    size_t size() const;

    /// Evict all expired entries (called periodically).
    void evict_expired();

private:
    struct Entry {
        std::string device_id;
        uint64_t seq;
        std::chrono::steady_clock::time_point seen_at;
    };

    size_t max_entries_;
    int    ttl_seconds_;

    std::list<Entry> lru_list_;
    std::unordered_map<std::string, std::list<Entry>::iterator> map_;
};

} // namespace signalroute
