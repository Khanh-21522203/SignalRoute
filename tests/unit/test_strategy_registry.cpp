#include "matching/match_context.h"
#include "matching/strategy_registry.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

class DummyStrategy final : public signalroute::IMatchStrategy {
public:
    explicit DummyStrategy(std::string strategy_name) : strategy_name_(std::move(strategy_name)) {}

    void initialize(const signalroute::Config&) override {}

    std::vector<std::string> match(
        const signalroute::MatchRequest&,
        const std::vector<signalroute::MatchCandidate>&,
        signalroute::MatchContext&) override {
        return {};
    }

    std::string name() const override { return strategy_name_; }

private:
    std::string strategy_name_;
};

void test_register_create_and_list_names() {
    auto& registry = signalroute::StrategyRegistry::instance();
    registry.register_strategy("unit.registry.z", [] {
        return std::make_unique<DummyStrategy>("z");
    });
    registry.register_strategy("unit.registry.a", [] {
        return std::make_unique<DummyStrategy>("a");
    });

    assert(registry.has("unit.registry.z"));
    auto strategy = registry.create("unit.registry.z");
    assert(strategy->name() == "z");

    auto names = registry.registered_names();
    assert(std::is_sorted(names.begin(), names.end()));
    assert(std::find(names.begin(), names.end(), "unit.registry.a") != names.end());
    assert(std::find(names.begin(), names.end(), "unit.registry.z") != names.end());
}

void test_invalid_registration_and_create_fail_clearly() {
    auto& registry = signalroute::StrategyRegistry::instance();

    bool empty_name_threw = false;
    try {
        registry.register_strategy("", [] { return std::make_unique<DummyStrategy>("bad"); });
    } catch (const std::invalid_argument&) {
        empty_name_threw = true;
    }
    assert(empty_name_threw);

    bool empty_factory_threw = false;
    try {
        registry.register_strategy("unit.registry.empty_factory", {});
    } catch (const std::invalid_argument&) {
        empty_factory_threw = true;
    }
    assert(empty_factory_threw);

    registry.register_strategy("unit.registry.null_factory", [] {
        return std::unique_ptr<signalroute::IMatchStrategy>{};
    });
    bool null_factory_threw = false;
    try {
        (void)registry.create("unit.registry.null_factory");
    } catch (const std::runtime_error&) {
        null_factory_threw = true;
    }
    assert(null_factory_threw);

    bool unknown_threw = false;
    try {
        (void)registry.create("unit.registry.missing");
    } catch (const std::runtime_error&) {
        unknown_threw = true;
    }
    assert(unknown_threw);
}

} // namespace

int main() {
    std::cout << "test_strategy_registry:\n";
    test_register_create_and_list_names();
    test_invalid_registration_and_create_fail_clearly();
    std::cout << "All strategy registry tests passed.\n";
    return 0;
}
