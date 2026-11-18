#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

// Deferred typed events: emit() queues, dispatch() delivers in emission order. No thread safety.
class EventBus {
    struct ChannelBase {
        virtual ~ChannelBase() = default;
        virtual void unsubscribe(std::uint64_t id) = 0;
    };

    template<typename E>
    struct Channel : ChannelBase {
        struct Handler {
            std::uint64_t id;
            bool active = true;
            std::function<void(const E&)> fn;
        };
        using List = std::vector<std::shared_ptr<Handler>>;
        std::shared_ptr<List> handlers = std::make_shared<List>();

        // Copy-on-write: a running deliver() holds the list, so it is copied only if it changes mid-delivery.
        List& edit() {
            if (handlers.use_count() > 1) handlers = std::make_shared<List>(*handlers);
            return *handlers;
        }

        void unsubscribe(std::uint64_t id) override {
            for (auto& h : *handlers) {
                if (h->id == id) h->active = false; // a running dispatch keeps its list and skips this one
            }
            std::erase_if(edit(), [](const auto& h) { return !h->active; });
        }

        void deliver(const E& e) {
            auto list = handlers; // handlers may (un)subscribe while we iterate
            for (auto& h : *list) {
                if (h->active) h->fn(e);
            }
        }
    };

public:
    // RAII, move-only. Safe to outlive the bus (the channel is held weakly).
    class Subscription {
    public:
        Subscription() = default;
        Subscription(std::weak_ptr<ChannelBase> channel, std::uint64_t id) : channel_(std::move(channel)), id_(id) {}
        Subscription(Subscription&& o) noexcept : channel_(std::move(o.channel_)), id_(o.id_) { o.channel_.reset(); }
        Subscription& operator=(Subscription&& o) noexcept {
            if (this != &o) {
                reset();
                channel_ = std::move(o.channel_);
                id_ = o.id_;
                o.channel_.reset();
            }
            return *this;
        }
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
        ~Subscription() { reset(); }

        void reset() {
            if (auto channel = channel_.lock()) channel->unsubscribe(id_);
            channel_.reset();
        }

    private:
        std::weak_ptr<ChannelBase> channel_;
        std::uint64_t id_ = 0;
    };

    static constexpr int MaxRounds = 8;

    template<typename E>
    [[nodiscard]] Subscription subscribe(std::function<void(const E&)> fn) {
        auto& ch = channel<E>();
        ch.edit().push_back(std::make_shared<typename Channel<E>::Handler>(typename Channel<E>::Handler{nextId_, true, std::move(fn)}));
        return Subscription{channels_[typeid(E)], nextId_++};
    }

    // E must be copy-constructible: the queued event is copied into every handler call.
    template<typename E>
    void emit(E event) {
        queue_.push_back([event = std::move(event)](EventBus& bus) { bus.channel<E>().deliver(event); });
    }

    // Events emitted by handlers go to the next round; MaxRounds per call, the rest waits for the next dispatch().
    void dispatch();

private:
    template<typename E>
    Channel<E>& channel() {
        auto& slot = channels_[typeid(E)];
        if (!slot) slot = std::make_shared<Channel<E>>();
        return static_cast<Channel<E>&>(*slot);
    }

    std::unordered_map<std::type_index, std::shared_ptr<ChannelBase>> channels_;
    std::vector<std::function<void(EventBus&)>> queue_;
    std::uint64_t nextId_ = 1;
};
