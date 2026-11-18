#include "EventBus.hpp"

#include <cstdio>

void EventBus::dispatch() {
    for (int round = 0; round < MaxRounds && !queue_.empty(); ++round) {
        std::vector<std::function<void(EventBus&)>> batch;
        batch.swap(queue_);
        for (auto& deliver : batch) deliver(*this);
    }
    if (!queue_.empty()) {
        std::fprintf(stderr, "EventBus: %zu events still queued after %d rounds, deferred to the next dispatch\n", queue_.size(), MaxRounds);
    }
}
