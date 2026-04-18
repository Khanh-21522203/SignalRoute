#include "strategy_registry.h"
#include <stdexcept>

namespace signalroute {

StrategyRegistry& StrategyRegistry::instance() {
    static StrategyRegistry registry;
    return registry;
}

void StrategyRegistry::register_strategy(const std::string& name, Factory factory) {
    factories_[name] = std::move(factory);
}

std::unique_ptr<IMatchStrategy> StrategyRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end()) {
        throw std::runtime_error("Unknown matching strategy: " + name);
    }
    return it->second();
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
    return names;
}

} // namespace signalroute
