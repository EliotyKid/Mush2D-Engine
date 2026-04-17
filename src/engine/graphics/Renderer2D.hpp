#pragma once

#include "../world/Scene2D.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class Renderer2D {
public:
    struct Vertex {
        float pos[2];
        float color[3];
        float uv[2];

        static VkVertexInputBindingDescription getBindingDescription() {
            VkVertexInputBindingDescription bindingDescription{};
            bindingDescription.binding = 0;
            bindingDescription.stride = sizeof(Vertex);
            bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            return bindingDescription;
        }

        static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
            std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

            attributeDescriptions[0].binding = 0;
            attributeDescriptions[0].location = 0;
            attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[0].offset = offsetof(Vertex, pos);

            attributeDescriptions[1].binding = 0;
            attributeDescriptions[1].location = 1;
            attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attributeDescriptions[1].offset = offsetof(Vertex, color);

            attributeDescriptions[2].binding = 0;
            attributeDescriptions[2].location = 2;
            attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
            attributeDescriptions[2].offset = offsetof(Vertex, uv);

            return attributeDescriptions;
        }
    };

    struct PushConstantData {
        glm::mat4 model{1.0f};
        glm::vec4 color{1.0f};
        glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f}; // x, y, w, h
    };

public:
    Renderer2D() = default;

    void init(
        VkDevice logicalDevice,
        VkPhysicalDevice physicalDevice,
        VkExtent2D swapChainExtent,
        VkRenderPass renderPass,
        VkDescriptorSetLayout descriptorSetLayout
    );

    void cleanup();

    void beginRender(VkCommandBuffer commandBuffer) const;

    void drawSprite(
        VkCommandBuffer commandBuffer,
        VkDescriptorSet descriptorSet,
        const Transform2D& transform,
        const glm::vec4& color,
        const glm::vec4& uvRect
    ) const;

    void renderScene(
        VkCommandBuffer commandBuffer,
        const Scene2D& scene,
        const std::vector<VkDescriptorSet>& descriptorSets
    ) const;

private:
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkExtent2D swapChainExtent{};
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
        {{ 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        {{ 0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
        {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}}
    };

    const std::vector<uint16_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

private:
    static std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code) const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void createGraphicsPipeline();
    void createVertexBuffer();
    void createIndexBuffer();

    glm::mat4 buildModelMatrix(const Transform2D& transform) const;
};