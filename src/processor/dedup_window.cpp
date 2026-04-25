#include "dedup_window.h"

namespace signalroute {

namespace {
std::string key_for(const std::string& device_id, uint64_t seq) {
    return device_id + "#" + std::to_string(seq);
}
} // namespace

DedupWindow::DedupWindow(size_t max_entries, int ttl_seconds)
    : max_entries_(max_entries), ttl_seconds_(ttl_seconds) {}

bool DedupWindow::is_duplicate(const std::string& device_id, uint64_t seq) {
    evict_expired();
    auto it = map_.find(key_for(device_id, seq));
    if (it == map_.end()) {
        return false;
    }

    it->second->seen_at = std::chrono::steady_clock::now();
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    return true;
}

void DedupWindow::mark_seen(const std::string& device_id, uint64_t seq) {
    evict_expired();
    const auto key = key_for(device_id, seq);
    auto it = map_.find(key);
    if (it != map_.end()) {
        it->second->seen_at = std::chrono::steady_clock::now();
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        return;
    }

    lru_list_.push_front({device_id, seq, std::chrono::steady_clock::now()});
    map_[key] = lru_list_.begin();

    while (map_.size() > max_entries_ && !lru_list_.empty()) {
        const auto& oldest = lru_list_.back();
        map_.erase(key_for(oldest.device_id, oldest.seq));
        lru_list_.pop_back();
    }
}

size_t DedupWindow::size() const { return map_.size(); }

void DedupWindow::evict_expired() {
    const auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(ttl_seconds_);
    while (!lru_list_.empty() && lru_list_.back().seen_at < cutoff) {
        const auto& oldest = lru_list_.back();
        map_.erase(key_for(oldest.device_id, oldest.seq));
        lru_list_.pop_back();
    }
}

} // namespace signalroute
