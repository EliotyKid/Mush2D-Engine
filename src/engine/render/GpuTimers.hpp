#pragma once

#include "engine/gpu/GpuContext.hpp"

#include <array>
#include <string>
#include <utility>
#include <vector>

// Per-pass GPU timestamps, one query range per frame in flight. Results of a slot are read when the
// slot is reused (2 frames later), without waiting. Empty when the queue has no timestamp support.
class GpuTimers {
public:
    static constexpr uint32_t MaxPasses = 8;

    void init(GpuContext& gpu);
    void shutdown();

    // Call first in the frame's command buffer: collects this slot's old results, then resets its queries.
    void beginFrame(VkCommandBuffer cmd);
    void begin(VkCommandBuffer cmd, const char* pass);
    void end(VkCommandBuffer cmd); // closes the pass opened by begin

    std::vector<std::pair<std::string, float>> lastFrameMs() const { return last_; }

private:
    struct Slot { std::vector<std::string> names; };

    GpuContext* gpu_ = nullptr;
    VkQueryPool pool_ = VK_NULL_HANDLE;
    float periodNs_ = 0;
    uint64_t mask_ = 0; // 0 = unsupported
    std::array<Slot, GpuContext::FramesInFlight> slots_{};
    std::vector<std::pair<std::string, float>> last_;
};