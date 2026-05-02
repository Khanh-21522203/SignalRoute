#include "integration_harness.h"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << "integration harness contract failed: " << message << '\n';
        std::exit(1);
    }
}

bool has_prefix(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

} // namespace

int main() {
    using signalroute::integration::kFeatureGroups;
    using signalroute::integration::is_feature_group;

    require(kFeatureGroups.size() == 6, "expected six production integration feature groups");

    for (const auto& group : kFeatureGroups) {
        require(!group.id.empty(), "feature group id must be non-empty");
        require(has_prefix(group.ctest_label, "integration:"), "ctest label must use integration namespace");
        require(!group.description.empty(), "feature group description must be non-empty");
        require(!group.required_services.empty(), "feature group must declare required services");
        require(!group.production_switches.empty(), "feature group must declare production switches");
        require(is_feature_group(group.id), "feature group lookup must find every manifest entry");
    }

    for (std::size_t i = 0; i < kFeatureGroups.size(); ++i) {
        for (std::size_t j = i + 1; j < kFeatureGroups.size(); ++j) {
            require(kFeatureGroups[i].id != kFeatureGroups[j].id, "feature group ids must be unique");
            require(kFeatureGroups[i].ctest_label != kFeatureGroups[j].ctest_label, "ctest labels must be unique");
        }
    }

    require(!is_feature_group("phase_1"), "phase-oriented integration groups are not allowed");
    require(!is_feature_group("batch_56"), "batch-oriented integration groups are not allowed");

    return 0;
}
