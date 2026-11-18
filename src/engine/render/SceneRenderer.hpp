#pragma once

#include "engine/gpu/Buffer.hpp"
#include "engine/gpu/GpuContext.hpp"
#include "engine/render/Bindless.hpp"
#include "engine/render/GpuTimers.hpp"
#include "engine/render/RenderTargets.hpp"
#include "engine/render/SpriteBatch.hpp"

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

class Camera2D;

struct GpuLight {                             // std430, 48 bytes
    glm::vec4 posHeightRadius;                // x, y, height, radius
    glm::vec4 colorIntensity;
    glm::vec4 depthFlicker;                   // depth, amplitude, speed, 0
};
static_assert(sizeof(GpuLight) == 48);

struct FrameSettings {
    glm::vec3 ambientColor{0.25f, 0.3f, 0.45f}; float ambientIntensity = 1.0f;
    glm::vec3 fogColor{0.55f, 0.65f, 0.8f};     float fogDensity = 0.08f;
    float fogGradientStart = 0, fogGradientEnd = 10, fogGradientStrength = 0;
    float exposure = 1.0f;
    float bloomThreshold = 1.0f, bloomKnee = 0.5f, bloomIntensity = 0.6f;
    glm::vec3 lift{0}, gamma{1}, gain{1}; float saturation = 1, temperature = 0;
    bool pixelArt = true; bool pixelateLight = false; int pixelateFactor = 2;
    bool lightHalfRes = false;
};

class LightList {
public:
    static constexpr size_t Max = 64;

    void clear() { count_ = 0; }
    bool add(const GpuLight& light) {
        if (count_ >= Max) return false;
        lights_[count_++] = light;
        return true;
    }
    size_t size() const { return count_; }
    const GpuLight* data() const { return lights_.data(); }

private:
    std::array<GpuLight, Max> lights_{};
    size_t count_ = 0;
};

// G-buffer -> deferred 2D light + fog (HDR) -> additive sprites -> bloom -> composite (bloom, exposure,
// temperature, AgX, lift/gamma/gain, saturation, sRGB) into the swapchain.
class SceneRenderer {
public:
    void init(GpuContext& gpu, Bindless& bindless);
    void shutdown();
    void resize(VkExtent2D extent);

    SpriteBatch& sprites() { return sprites_; }
    LightList& lights() { return lights_; }
    FrameSettings& settings() { return settings_; }
    const GpuTimers& timers() const { return timers_; }

    // Leaves the swapchain image in PRESENT_SRC_KHR.
    void render(FrameContext& frame, const Camera2D& camera, float time);

private:
    struct FrameResources {
        GpuBuffer uniforms, instances, lights;
        VkDescriptorSet set = VK_NULL_HANDLE;
    };

    VkExtent2D lightExtent(VkExtent2D extent) const;
    void writeFrameSet(FrameResources& fr);
    VkPipeline createPipeline(const char* vert, const char* frag, const VkFormat* formats, uint32_t count,
                             bool additiveBlend = false);

    GpuContext* gpu_ = nullptr;
    Bindless* bindless_ = nullptr;
    SpriteBatch sprites_;
    LightList lights_;
    FrameSettings settings_;
    RenderTargets targets_;

    VkDescriptorSetLayout frameLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline gbufferPipeline_ = VK_NULL_HANDLE;
    VkPipeline lightPipeline_ = VK_NULL_HANDLE;
    VkPipeline additivePipeline_ = VK_NULL_HANDLE;
    VkPipeline bloomDownPipeline_ = VK_NULL_HANDLE;
    VkPipeline bloomUpPipeline_ = VK_NULL_HANDLE;
    VkPipeline compositePipeline_ = VK_NULL_HANDLE;
    GpuTimers timers_;
    float logTimer_ = 0;
    std::array<FrameResources, GpuContext::FramesInFlight> frames_{};
};
