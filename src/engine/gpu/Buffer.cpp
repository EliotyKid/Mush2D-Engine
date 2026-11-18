#include "engine/gpu/Buffer.hpp"

#include <stdexcept>

GpuBuffer createHostBuffer(GpuContext& gpu, VkDeviceSize size, VkBufferUsageFlags usage,
                           VmaAllocationCreateFlags hostAccess) {
    VkBufferCreateInfo bci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bci.size = size;
    bci.usage = usage;
    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = hostAccess | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    GpuBuffer buf;
    VmaAllocationInfo info{};
    if (vmaCreateBuffer(gpu.allocator(), &bci, &aci, &buf.buffer, &buf.alloc, &info) != VK_SUCCESS) {
        throw std::runtime_error("createHostBuffer: vmaCreateBuffer falhou.");
    }
    buf.mapped = info.pMappedData;
    buf.size = size;
    return buf;
}

void destroyBuffer(GpuContext& gpu, GpuBuffer& buf) {
    if (buf.buffer != VK_NULL_HANDLE) vmaDestroyBuffer(gpu.allocator(), buf.buffer, buf.alloc);
    buf = {};
}
