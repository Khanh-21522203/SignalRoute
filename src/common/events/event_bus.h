#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <mutex>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace signalroute {

class EventBus {
public:
    class Subscription {
    public:
        Subscription() = default;
        Subscription(EventBus* bus, std::type_index type, std::size_t id)
            : bus_(bus), type_(type), id_(id) {}

        ~Subscription() { reset(); }

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

        Subscription(Subscription&& other) noexcept
            : bus_(std::exchange(other.bus_, nullptr)), type_(other.type_), id_(other.id_) {}

        Subscription& operator=(Subscription&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            reset();
            bus_ = std::exchange(other.bus_, nullptr);
            type_ = other.type_;
            id_ = other.id_;
            return *this;
        }

        void reset() {
            if (bus_ == nullptr) {
                return;
            }
            bus_->unsubscribe(type_, id_);
            bus_ = nullptr;
        }

        [[nodiscard]] bool active() const { return bus_ != nullptr; }

    private:
        EventBus* bus_ = nullptr;
        std::type_index type_{typeid(void)};
        std::size_t id_ = 0;
    };

    template <typename Event>
    Subscription subscribe(std::function<void(const Event&)> handler) {
        std::lock_guard lock(mu_);
        const auto id = next_id_++;
        handlers_[std::type_index(typeid(Event))].push_back(HandlerEntry{
            id,
            [handler = std::move(handler)](const void* event) {
                handler(*static_cast<const Event*>(event));
            }});
        return Subscription(this, std::type_index(typeid(Event)), id);
    }

    template <typename Event, typename Handler>
    Subscription subscribe(Handler&& handler) {
        return subscribe<Event>(std::function<void(const Event&)>(std::forward<Handler>(handler)));
    }

    template <typename Event>
    void publish(const Event& event) const {
        std::vector<Callback> callbacks;
        {
            std::lock_guard lock(mu_);
            auto it = handlers_.find(std::type_index(typeid(Event)));
            if (it == handlers_.end()) {
                return;
            }
            callbacks.reserve(it->second.size());
            for (const auto& entry : it->second) {
                callbacks.push_back(entry.callback);
            }
        }

        for (const auto& callback : callbacks) {
            callback(&event);
        }
    }

    template <typename Event>
    [[nodiscard]] std::size_t subscriber_count() const {
        std::lock_guard lock(mu_);
        auto it = handlers_.find(std::type_index(typeid(Event)));
        return it == handlers_.end() ? 0 : it->second.size();
    }

private:
    using Callback = std::function<void(const void*)>;

    struct HandlerEntry {
        std::size_t id;
        Callback callback;
    };

    void unsubscribe(std::type_index type, std::size_t id) {
        std::lock_guard lock(mu_);
        auto it = handlers_.find(type);
        if (it == handlers_.end()) {
            return;
        }

        auto& entries = it->second;
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [id](const HandlerEntry& entry) { return entry.id == id; }),
            entries.end());
        if (entries.empty()) {
            handlers_.erase(it);
        }
    }

    mutable std::mutex mu_;
    std::size_t next_id_ = 1;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
};

} // namespace signalroute
