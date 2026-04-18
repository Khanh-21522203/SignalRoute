#include "fence_registry.h"

namespace signalroute {

void FenceRegistry::load(PostgresClient& pg) {
    auto rules = pg.load_active_fences();

    std::unique_lock lock(mu_);
    fences_.clear();
    for (auto& rule : rules) {
        fences_[rule.fence_id] = std::move(rule);
    }
    rebuild_index();
}

void FenceRegistry::reload(PostgresClient& pg) {
    // TODO: Implement diff-based reload
    // For now, full reload
    load(pg);
}

std::vector<const GeofenceRule*> FenceRegistry::get_candidates(int64_t h3_cell) const {
    std::shared_lock lock(mu_);
    std::vector<const GeofenceRule*> result;
    auto it = cell_to_fences_.find(h3_cell);
    if (it != cell_to_fences_.end()) {
        for (const auto& fence_id : it->second) {
            auto fit = fences_.find(fence_id);
            if (fit != fences_.end()) {
                result.push_back(&fit->second);
            }
        }
    }
    return result;
}

size_t FenceRegistry::fence_count() const {
    std::shared_lock lock(mu_);
    return fences_.size();
}

void FenceRegistry::rebuild_index() {
    cell_to_fences_.clear();
    for (const auto& [fence_id, rule] : fences_) {
        for (int64_t cell : rule.h3_cells) {
            cell_to_fences_[cell].push_back(fence_id);
        }
    }
}

} // namespace signalroute
