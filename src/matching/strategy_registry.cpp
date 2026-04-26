#include "strategy_registry.h"
#include <algorithm>
#include <stdexcept>

namespace signalroute {

StrategyRegistry& StrategyRegistry::instance() {
    static StrategyRegistry registry;
    return registry;
}

void StrategyRegistry::register_strategy(const std::string& name, Factory factory) {
    if (name.empty()) {
        throw std::invalid_argument("matching strategy name must not be empty");
    }
    if (!factory) {
        throw std::invalid_argument("matching strategy factory must not be empty");
    }
    factories_[name] = std::move(factory);
}

std::unique_ptr<IMatchStrategy> StrategyRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end()) {
        throw std::runtime_error("Unknown matching strategy: " + name);
    }
    auto strategy = it->second();
    if (!strategy) {
        throw std::runtime_error("Matching strategy factory returned null: " + name);
    }
    return strategy;
}

bool StrategyRegistry::has(const std::string& name) const {
    return factories_.count(name) > 0;
}

std::vector<std::string> StrategyRegistry::registered_names() const {
    std::vector<std::string> names;
    names.reserve(factories_.size());
    for (const auto& [name, _] : factories_) {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace signalroute
