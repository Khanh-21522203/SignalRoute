#include "processor/dedup_window.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

void test_detects_seen_event() {
    signalroute::DedupWindow dedup(10, 60);
    assert(!dedup.is_duplicate("dev-1", 1));
    dedup.mark_seen("dev-1", 1);
    assert(dedup.is_duplicate("dev-1", 1));
    assert(!dedup.is_duplicate("dev-1", 2));
}

void test_evicts_lru_when_capacity_is_reached() {
    signalroute::DedupWindow dedup(2, 60);
    dedup.mark_seen("dev-1", 1);
    dedup.mark_seen("dev-1", 2);
    dedup.mark_seen("dev-1", 3);

    assert(dedup.size() == 2);
    assert(!dedup.is_duplicate("dev-1", 1));
    assert(dedup.is_duplicate("dev-1", 2));
    assert(dedup.is_duplicate("dev-1", 3));
}

void test_evicts_expired_entries() {
    signalroute::DedupWindow dedup(10, 0);
    dedup.mark_seen("dev-1", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    dedup.evict_expired();
    assert(dedup.size() == 0);
}

int main() {
    std::cout << "test_dedup_window:\n";
    test_detects_seen_event();
    test_evicts_lru_when_capacity_is_reached();
    test_evicts_expired_entries();
    std::cout << "All dedup window tests passed.\n";
    return 0;
}
