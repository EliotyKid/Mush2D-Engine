#include "engine/render/SceneRenderer.hpp"

#include "engine/core/Paths.hpp"
#include "engine/graphics/Camera2D.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr VkDeviceSize InitialInstanceCapacity = 1024;

// std140; mirrors `FrameUniforms` in shaders/common.glsl.
struct FrameUniforms {
    glm::mat4 viewProj;
    glm::vec2 cameraPos;
    glm::vec2 viewport;
    glm::vec2 halfSize; // visible half extent in world units
    float pixelsPerUnit;
    float time;
    uint32_t lightCount;
    uint32_t pixelArt;
    float fogDensity;
    float ambientIntensity;
    glm::vec3 ambientColor; float exposure;
    glm::vec3 fogColor;     float fogGradientStart;
    float fogGradientEnd;
    float fogGradientStrength;
    float _pad[2];
};
static_assert(offsetof(FrameUniforms, lightCount) == 96);
static_assert(offsetof(FrameUniforms, ambientColor) == 112);
static_assert(offsetof(FrameUniforms, fogColor) == 128);
static_assert(offsetof(FrameUniforms, fogGradientEnd) == 144);
static_assert(sizeof(FrameUniforms) == 160);

// Shared by every pipeline: firstInstance for sprites, t0..t3 = bindless indices / sampler choice.
struct PushConstants {
    uint32_t firstInstance = 0;
    uint32_t t0 = 0, t1 = 0, t2 = 0, t3 = 0;
};

void check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) throw std::runtime_error(std::string("SceneRenderer: ") + what + " falhou.");
}

VkShaderModule loadShader(VkDevice device, const char* name) {
    auto path = assetPath(std::string("shaders/") + name + ".spv");
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("SceneRenderer: shader nao encontrado: " + path.string());
    std::vector<char> code(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(code.size()));

    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    check(vkCreateShaderModule(device, &ci, nullptr, &module), "vkCreateShaderModule");
    return module;
}

void flush(GpuContext& gpu, const GpuBuffer& buf) {
    vmaFlushAllocation(gpu.allocator(), buf.alloc, 0, VK_WHOLE_SIZE);
}

void beginRendering(VkCommandBuffer cmd, VkExtent2D extent, const VkRenderingAttachmentInfo* attachments, uint32_t count) {
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, extent};
    ri.layerCount = 1;
    ri.colorAttachmentCount = count;
    ri.pColorAttachments = attachments;
    vkCmdBeginRendering(cmd, &ri);

    VkViewport vp{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

VkRenderingAttachmentInfo attachment(VkImageView view, VkClearColorValue clear) {
    VkRenderingAttachmentInfo a{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    a.imageView = view;
    a.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a.clearValue.color = clear;
    return a;
}
}

void SceneRenderer::init(GpuContext& gpu, Bindless& bindless) {
    gpu_ = &gpu;
    bindless_ = &bindless;
    VkDevice device = gpu.device();

    // set 1: per-frame uniforms, instances, lights.
    VkDescriptorSetLayoutBinding b[3]{};
    VkDescriptorType types[3] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER};
    for (uint32_t i = 0; i < 3; ++i) {
        b[i].binding = i;
        b[i].descriptorType = types[i];
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.bindingCount = 3;
    lci.pBindings = b;
    check(vkCreateDescriptorSetLayout(device, &lci, nullptr, &frameLayout_), "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, GpuContext::FramesInFlight},
                                     {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * GpuContext::FramesInFlight}};
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.maxSets = GpuContext::FramesInFlight;
    pci.poolSizeCount = 2;
    pci.pPoolSizes = sizes;
    check(vkCreateDescriptorPool(device, &pci, nullptr, &pool_), "vkCreateDescriptorPool");

    for (FrameResources& fr : frames_) {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = pool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &frameLayout_;
        check(vkAllocateDescriptorSets(device, &ai, &fr.set), "vkAllocateDescriptorSets");
        fr.uniforms = createHostBuffer(gpu, sizeof(FrameUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        fr.instances = createHostBuffer(gpu, InitialInstanceCapacity * sizeof(SpriteInstance),
                                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        fr.lights = createHostBuffer(gpu, LightList::Max * sizeof(GpuLight), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        writeFrameSet(fr);
    }

    VkDescriptorSetLayout setLayouts[2] = {bindless.layout(), frameLayout_};
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    plci.setLayoutCount = 2;
    plci.pSetLayouts = setLayouts;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &push;
    check(vkCreatePipelineLayout(device, &plci, nullptr, &pipelineLayout_), "vkCreatePipelineLayout");

    const VkFormat* f = RenderTargets::Formats.data();
    gbufferPipeline_ = createPipeline("sprite.vert", "gbuffer.frag", f, 4);
    lightPipeline_ = createPipeline("fullscreen.vert", "light.frag", f + RenderTargets::Light, 1);
    VkFormat swapFormat = gpu.swapchain().format();
    compositePipeline_ = createPipeline("fullscreen.vert", "composite.frag", &swapFormat, 1);

    VkExtent2D extent = gpu.swapchain().extent();
    targets_.create(gpu, bindless, extent, lightExtent(extent));
}

void SceneRenderer::shutdown() {
    if (!gpu_) return;
    VkDevice device = gpu_->device();
    vkDeviceWaitIdle(device);
    targets_.destroy(*gpu_);
    for (FrameResources& fr : frames_) {
        destroyBuffer(*gpu_, fr.uniforms);
        destroyBuffer(*gpu_, fr.instances);
        destroyBuffer(*gpu_, fr.lights);
    }
    vkDestroyPipeline(device, gbufferPipeline_, nullptr);
    vkDestroyPipeline(device, lightPipeline_, nullptr);
    vkDestroyPipeline(device, compositePipeline_, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout_, nullptr);
    vkDestroyDescriptorPool(device, pool_, nullptr); // frees the sets
    vkDestroyDescriptorSetLayout(device, frameLayout_, nullptr);
    *this = {};
}

void SceneRenderer::resize(VkExtent2D extent) {
    vkDeviceWaitIdle(gpu_->device());
    targets_.create(*gpu_, *bindless_, extent, lightExtent(extent));
}

VkExtent2D SceneRenderer::lightExtent(VkExtent2D extent) const {
    if (!settings_.lightHalfRes && !settings_.pixelateLight) return extent;
    uint32_t div = settings_.pixelateLight ? static_cast<uint32_t>(std::max(1, settings_.pixelateFactor)) : 2u;
    return {std::max(1u, extent.width / div), std::max(1u, extent.height / div)};
}

void SceneRenderer::writeFrameSet(FrameResources& fr) {
    VkDescriptorBufferInfo infos[3] = {{fr.uniforms.buffer, 0, VK_WHOLE_SIZE},
                                       {fr.instances.buffer, 0, VK_WHOLE_SIZE},
                                       {fr.lights.buffer, 0, VK_WHOLE_SIZE}};
    VkWriteDescriptorSet w[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = fr.set;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w[i].pBufferInfo = &infos[i];
    }
    vkUpdateDescriptorSets(gpu_->device(), 3, w, 0, nullptr);
}

VkPipeline SceneRenderer::createPipeline(const char* vert, const char* frag, const VkFormat* formats, uint32_t count) {
    VkDevice device = gpu_->device();
    VkShaderModule vs = loadShader(device, vert);
    VkShaderModule fs = loadShader(device, frag);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vs, "main"};
    stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main"};

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend[4]{};
    for (auto& b : blend) b.colorWriteMask = 0xF; // no blending
    VkPipelineColorBlendStateCreateInfo blendState{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blendState.attachmentCount = count;
    blendState.pAttachments = blend;

    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dyn;

    VkPipelineRenderingCreateInfo rendering{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rendering.colorAttachmentCount = count;
    rendering.pColorAttachmentFormats = formats;

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &rendering;
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vertexInput;
    ci.pInputAssemblyState = &assembly;
    ci.pViewportState = &viewport;
    ci.pRasterizationState = &raster;
    ci.pMultisampleState = &multisample;
    ci.pColorBlendState = &blendState;
    ci.pDynamicState = &dynamic;
    ci.layout = pipelineLayout_;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult r = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline);
    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, fs, nullptr);
    check(r, "vkCreateGraphicsPipelines");
    return pipeline;
}

void SceneRenderer::render(FrameContext& frame, const Camera2D& camera, float time) {
    GpuContext& gpu = *gpu_;
    VkExtent2D extent = gpu.swapchain().extent();
    VkExtent2D lightExt = lightExtent(extent);
    if (targets_.extent().width != extent.width || targets_.extent().height != extent.height ||
        targets_.lightExtent().width != lightExt.width || targets_.lightExtent().height != lightExt.height) {
        resize(extent); // light resolution settings changed
    }

    // This frame's fence was waited in beginFrame, so its buffers and set are free to rewrite.
    FrameResources& fr = frames_[gpu.frameIndex()];
    const std::vector<SpriteInstance>& instances = sprites_.sorted();
    VkDeviceSize bytes = instances.size() * sizeof(SpriteInstance);
    if (bytes > fr.instances.size) {
        VkDeviceSize capacity = fr.instances.size;
        while (capacity < bytes) capacity *= 2;
        destroyBuffer(gpu, fr.instances);
        fr.instances = createHostBuffer(gpu, capacity, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        writeFrameSet(fr);
    }
    if (bytes) std::memcpy(fr.instances.mapped, instances.data(), static_cast<size_t>(bytes));
    flush(gpu, fr.instances);
    std::memcpy(fr.lights.mapped, lights_.data(), lights_.size() * sizeof(GpuLight));
    flush(gpu, fr.lights);

    FrameUniforms u{};
    u.viewProj = camera.buildProjectionMatrix(extent);
    u.cameraPos = camera.position;
    u.viewport = {static_cast<float>(extent.width), static_cast<float>(extent.height)};
    u.halfSize = camera.halfExtent(extent);
    u.pixelsPerUnit = camera.pixelsPerUnit;
    u.time = time;
    u.lightCount = static_cast<uint32_t>(lights_.size());
    u.pixelArt = settings_.pixelArt ? 1u : 0u;
    u.fogDensity = settings_.fogDensity;
    u.ambientIntensity = settings_.ambientIntensity;
    u.ambientColor = settings_.ambientColor;
    u.exposure = settings_.exposure;
    u.fogColor = settings_.fogColor;
    u.fogGradientStart = settings_.fogGradientStart;
    u.fogGradientEnd = settings_.fogGradientEnd;
    u.fogGradientStrength = settings_.fogGradientStrength;
    std::memcpy(fr.uniforms.mapped, &u, sizeof(u));
    flush(gpu, fr.uniforms);

    VkCommandBuffer cmd = frame.cmd;
    VkDescriptorSet sets[2] = {bindless_->descriptorSet(), fr.set};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 2, sets, 0, nullptr);
    auto push = [&](const PushConstants& pc) {
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(pc), &pc);
    };

    // 1. G-buffer. Albedo alpha 0 marks "no sprite" (sky) for the light pass.
    VkRenderingAttachmentInfo g[4] = {
        attachment(targets_.image(RenderTargets::Albedo).view, {{0.0f, 0.0f, 0.0f, 0.0f}}),
        attachment(targets_.image(RenderTargets::Normal).view, {{0.5f, 0.5f, 0.0f, 1.0f}}),
        attachment(targets_.image(RenderTargets::Emissive).view, {{0.0f, 0.0f, 0.0f, 0.0f}}),
        attachment(targets_.image(RenderTargets::Depth).view, {{0.0f, 0.0f, 0.0f, 0.0f}}),
    };
    for (int t = RenderTargets::Albedo; t <= RenderTargets::Depth; ++t) {
        transition(cmd, targets_.image(static_cast<RenderTargets::Target>(t)).image, VK_IMAGE_LAYOUT_UNDEFINED,
                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    beginRendering(cmd, extent, g, 4);
    if (!instances.empty()) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferPipeline_);
        push({});
        vkCmdDraw(cmd, 6, static_cast<uint32_t>(instances.size()), 0, 0);
    }
    vkCmdEndRendering(cmd);
    for (int t = RenderTargets::Albedo; t <= RenderTargets::Depth; ++t) {
        transition(cmd, targets_.image(static_cast<RenderTargets::Target>(t)).image,
                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // 2. Light + fog into HDR (possibly reduced resolution).
    const GpuImage& light = targets_.image(RenderTargets::Light);
    transition(cmd, light.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo l = attachment(light.view, {{0.0f, 0.0f, 0.0f, 1.0f}});
    l.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; // fully overwritten
    beginRendering(cmd, light.extent, &l, 1);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipeline_);
    push({0, targets_.index(RenderTargets::Albedo), targets_.index(RenderTargets::Normal),
          targets_.index(RenderTargets::Emissive), targets_.index(RenderTargets::Depth)});
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
    transition(cmd, light.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // 3. Composite into the swapchain (sampler 0 = NEAREST keeps pixelated light blocky).
    VkImage swapImage = gpu.swapchain().image(frame.imageIndex);
    transition(cmd, swapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo c = attachment(gpu.swapchain().view(frame.imageIndex), {{0.0f, 0.0f, 0.0f, 1.0f}});
    c.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    beginRendering(cmd, extent, &c, 1);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline_);
    push({0, targets_.index(RenderTargets::Light), settings_.pixelateLight ? 0u : 1u, 0, 0});
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);
    transition(cmd, swapImage, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}
