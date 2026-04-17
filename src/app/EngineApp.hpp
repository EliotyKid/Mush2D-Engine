#pragma once

#include "../game/objects/CheckpointObject.hpp"
#include "../game/objects/PlayerObject.hpp"
#include "../game/objects/SlimeEnemy.hpp"
#include "../game/objects/TeleportObject.hpp"
#include "../game/objects/WallObject.hpp"
#include "../engine/graphics/Camera2D.hpp"
#include "../engine/graphics/Renderer2D.hpp"
#include "../engine/platform/GlfwInput.hpp"
#include "../engine/world/Collision.hpp"
#include "../engine/world/Scene2D.hpp"
#include "../game/spawners/Spawn.hpp"
#include "../engine/scenes/ISceneDefinition.hpp"
#include "../game/scenes/SandboxScene.hpp"
#include "../game/scenes/TestScene.hpp"
#include "../engine/scenes/SceneRegistry.hpp"
#include "../engine/scenes/SceneId.hpp"
#include "../game/assets/GameAssets.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

class EngineApp {
public:
    void run();

private:
    static constexpr uint32_t WIDTH = 1280;
    static constexpr uint32_t HEIGHT = 720;

    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct UniformBufferObject {
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
    };

    struct TextureResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };

private:
    GLFWwindow* window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice logicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;
    std::vector<VkCommandBuffer> commandBuffers;
    VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapChainExtent{};

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    VkBuffer uniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory uniformBufferMemory = VK_NULL_HANDLE;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    Renderer2D renderer2D;
    Camera2D camera{};
    GlfwInput input;

    Scene2D scene{};
    std::vector<TextureResource> textures;

    GameObjectId playerObjectId = InvalidGameObjectId;

    float mouseZoomSensitivity = 0.15f;
    float cameraMoveSpeed = 1.5f;
    float cameraZoomSpeed = 1.0f;

    std::unordered_set<GameObjectId> activeTriggerOverlaps;

    std::unique_ptr<ISceneDefinition> activeSceneDefinition;

    SceneId currentSceneId = SceneId::Sandbox;
    bool nextScenePressed = false;

    bool reloadScenePressed = false;

    SceneTransitionData pendingTransitionData{};
    SceneTransitionData currentSceneTransitionData{};

    GameAssets gameAssets;

private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createDescriptorSetLayout();
    void createUniformBuffer();
    void createDescriptorPool();

    std::vector<VkCommandBuffer> createCommandBuffers();
    void createSyncObjects();

    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void handleGlobalInput(float deltaTime);
    void processTriggerOverlaps();
    void setupInitialScene();
    void updateUniformBuffer();

    void reloadCurrentScene();

    std::vector<VkDescriptorSet> getTextureDescriptorSets() const;

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const;

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    void createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory
    );

    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) const;

    void transitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    TextureResource createTextureResource(const char* path);
    VkDescriptorSet createTextureDescriptorSet(VkImageView imageView, VkSampler sampler);
    void loadTextures();

    void loadScene(SceneId sceneId, const SceneTransitionData& transitionData = {});
};