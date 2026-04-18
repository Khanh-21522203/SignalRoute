#include "dedup_window.h"

namespace signalroute {

DedupWindow::DedupWindow(size_t max_entries, int ttl_seconds)
    : max_entries_(max_entries), ttl_seconds_(ttl_seconds) {}

bool DedupWindow::is_duplicate(const std::string& /*device_id*/, uint64_t /*seq*/) {
    // TODO: Implement LRU lookup
    return false;
}

void DedupWindow::mark_seen(const std::string& /*device_id*/, uint64_t /*seq*/) {
    // TODO: Implement LRU insert + eviction
}

size_t DedupWindow::size() const { return 0; }

void DedupWindow::evict_expired() {
    // TODO: Walk LRU list tail-to-head, remove entries older than ttl_seconds_
}

} // namespace signalroute
