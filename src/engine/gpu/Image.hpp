#pragma once

#include "engine/gpu/GpuContext.hpp"

struct GpuImage {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
};

GpuImage createImage(GpuContext& gpu, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);
void destroyImage(GpuContext& gpu, GpuImage& img);

// Conservative: ALL_COMMANDS, MEMORY_WRITE -> MEMORY_READ|WRITE, color aspect, 1 mip, 1 layer.
void transition(VkCommandBuffer cmd, VkImage image, VkImageLayout from, VkImageLayout to);
