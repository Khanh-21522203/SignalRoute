#pragma once

/**
 * SignalRoute — Strategy Registry
 *
 * Maps strategy names to factory functions for creating IMatchStrategy instances.
 * Strategies are registered at compile time via static initialization.
 */

#include "strategy_interface.h"
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>

namespace signalroute {

class StrategyRegistry {
public:
    using Factory = std::function<std::unique_ptr<IMatchStrategy>()>;

    static StrategyRegistry& instance();

    /// Register a strategy factory by name.
    void register_strategy(const std::string& name, Factory factory);

    /// Create a strategy by name.
    std::unique_ptr<IMatchStrategy> create(const std::string& name) const;

    /// Check if a strategy is registered.
    bool has(const std::string& name) const;

    /// Registered strategy names (for diagnostics).
    std::vector<std::string> registered_names() const;

private:
    StrategyRegistry() = default;
    std::unordered_map<std::string, Factory> factories_;
};

/**
 * Helper macro for static strategy registration.
 *
 * Usage in a .cpp file:
 *   REGISTER_STRATEGY("nearest", NearestStrategy);
 */
#define REGISTER_STRATEGY(name, StrategyClass)                              \
    static bool _reg_##StrategyClass = [] {                                 \
        signalroute::StrategyRegistry::instance().register_strategy(        \
            name, [] { return std::make_unique<StrategyClass>(); });         \
        return true;                                                         \
    }()

} // namespace signalroute
