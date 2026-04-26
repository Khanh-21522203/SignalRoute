#include "common/clients/redis_client.h"
#include "matching/reservation_manager.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

void test_reserve_release_and_ownership() {
    signalroute::RedisConfig config;
    signalroute::RedisClient redis(config);
    signalroute::ReservationManager reservations(redis, 5000);

    assert(!reservations.is_reserved("agent-1"));
    assert(reservations.reserve("agent-1", "request-1"));
    assert(reservations.is_reserved("agent-1"));
    assert(redis.get_agent_reservation_holder("agent-1") == "request-1");

    assert(!reservations.reserve("agent-1", "request-2"));
    reservations.release("agent-1", "request-2");
    assert(reservations.is_reserved("agent-1"));

    reservations.release("agent-1", "request-1");
    assert(!reservations.is_reserved("agent-1"));
    assert(reservations.reserve("agent-1", "request-2"));
}

void test_empty_ids_and_invalid_ttl_are_rejected() {
    signalroute::RedisConfig config;
    signalroute::RedisClient redis(config);
    signalroute::ReservationManager invalid_ttl(redis, 0);
    signalroute::ReservationManager reservations(redis, 5000);

    assert(!reservations.reserve("", "request-1"));
    assert(!reservations.reserve("agent-1", ""));
    assert(!invalid_ttl.reserve("agent-2", "request-1"));
    assert(!reservations.is_reserved(""));
}

void test_reservation_expires() {
    signalroute::RedisConfig config;
    signalroute::RedisClient redis(config);
    signalroute::ReservationManager reservations(redis, 1);

    assert(reservations.reserve("agent-1", "request-1"));
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    assert(!reservations.is_reserved("agent-1"));
    assert(!redis.get_agent_reservation_holder("agent-1").has_value());
    assert(reservations.reserve("agent-1", "request-2"));
}

int main() {
    std::cout << "test_reservation_manager:\n";
    test_reserve_release_and_ownership();
    test_empty_ids_and_invalid_ttl_are_rejected();
    test_reservation_expires();
    std::cout << "All reservation manager tests passed.\n";
    return 0;
}
