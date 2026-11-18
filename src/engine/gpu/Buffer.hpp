#pragma once

#include "engine/gpu/GpuContext.hpp"

// Persistently mapped host-visible buffer (VMA picks the memory type).
struct GpuBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation alloc = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
};

// hostAccess: VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT (upload) or _RANDOM_BIT (readback).
GpuBuffer createHostBuffer(GpuContext& gpu, VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags hostAccess = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
void destroyBuffer(GpuContext& gpu, GpuBuffer& buf);
