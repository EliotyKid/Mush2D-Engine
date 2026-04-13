#pragma once

#include "GameObject.hpp"
#include "Renderer2D.hpp"

#include <cstdint>
#include <glm/ext/vector_float2.hpp>
#include <optional>
#include <set>
#include <vector>

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

    struct Camera2D {
        glm::vec2 position{0.0f, 0.0f};
        float size = 2.0f;
        GameObjectId targetId = InvalidGameObjectId;
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

    struct WorldBounds {
        float left = -5.0f;
        float right = 5.0f;
        float bottom = -3.0f;
        float top = 3.0f;
    };

    struct Player {
        glm::vec2 position{0.0f, 0.0f};
        glm::vec2 velocity{0.0f, 0.0f};
        float moveSpeed = 2.5f;
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

    std::vector<TextureResource> textures;

    Renderer2D renderer2D;

    Camera2D camera{
        {0.0f, 0.0f},
        2.0f
    };

    Scene2D scene{};

    float cameraMoveSpeed = 1.5f;
    float cameraZoomSpeed = 1.0f;

    bool isDraggingCamera = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    float mouseZoomSensitivity = 0.15f;
    int dragMouseButton = GLFW_MOUSE_BUTTON_MIDDLE;

    bool cameraFollowEnabled = true;
    glm::vec2 cameraFollowOffset{0.0f, 0.0f};

    bool followTogglePressed = false;

    float cameraFollowSmoothness = 6.0f;
    float cameraSnapDistance = 0.001f;

    WorldBounds worldBounds{
        -5.0f,
        5.0f,
        -3.0f,
        3.0f
    };
    bool cameraBoundsEnabled = true;

    Player player{
        {0.0f, 0.0f},
        {0.0f, 0.0f},
        2.5f
    };
    GameObjectId playerObjectId = InvalidGameObjectId;

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

    TextureResource createTextureResource(const char* path);
    VkDescriptorSet createTextureDescriptorSet(VkImageView imageView, VkSampler sampler);
    void loadTextures();

    std::vector<VkCommandBuffer> createCommandBuffers();
    void createSyncObjects();

    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void updateScene();
    void updateUniformBuffer();

    glm::mat4 buildViewMatrix() const;
    glm::mat4 buildProjectionMatrix() const;

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

    std::vector<VkDescriptorSet> getTextureDescriptorSets() const;

    void handleInput(float deltaTime);

    void setupInputCallbacks();
    void onMouseScroll(double xOffset, double yOffset);
    void onMouseButton(int button, int action, int mods);
    void onMouseMove(double xpos, double ypos);

    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);

    void updateCameraFollow(float deltaTime);

    void clampCameraToBounds();

    void updatePlayer(float deltaTime);

    void setupInitialScene();
};