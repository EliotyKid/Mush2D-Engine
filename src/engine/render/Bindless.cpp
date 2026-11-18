#include "engine/render/Bindless.hpp"

#include <stdexcept>

void Bindless::init(GpuContext& gpu) {
    device_ = gpu.device();

    for (int i = 0; i < 2; ++i) {
        VkSamplerCreateInfo sci{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sci.magFilter = sci.minFilter = (i == 0) ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        if (vkCreateSampler(device_, &sci, nullptr, &samplers_[i]) != VK_SUCCESS) {
            throw std::runtime_error("Bindless: vkCreateSampler falhou.");
        }
    }

    VkDescriptorSetLayoutBinding b[2]{};
    b[0].binding = 0;
    b[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    b[0].descriptorCount = Capacity;
    b[0].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    b[1].binding = 1;
    b[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    b[1].descriptorCount = 2;
    b[1].stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS;
    b[1].pImmutableSamplers = samplers_;

    VkDescriptorBindingFlags flags[2] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, 0};
    VkDescriptorSetLayoutBindingFlagsCreateInfo bfi{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bfi.bindingCount = 2;
    bfi.pBindingFlags = flags;

    VkDescriptorSetLayoutCreateInfo lci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lci.pNext = &bfi;
    lci.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    lci.bindingCount = 2;
    lci.pBindings = b;
    if (vkCreateDescriptorSetLayout(device_, &lci, nullptr, &layout_) != VK_SUCCESS) {
        throw std::runtime_error("Bindless: vkCreateDescriptorSetLayout falhou.");
    }

    VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, Capacity}, {VK_DESCRIPTOR_TYPE_SAMPLER, 2}};
    VkDescriptorPoolCreateInfo pci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pci.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pci.maxSets = 1;
    pci.poolSizeCount = 2;
    pci.pPoolSizes = sizes;
    if (vkCreateDescriptorPool(device_, &pci, nullptr, &pool_) != VK_SUCCESS) {
        throw std::runtime_error("Bindless: vkCreateDescriptorPool falhou.");
    }

    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &layout_;
    if (vkAllocateDescriptorSets(device_, &ai, &set_) != VK_SUCCESS) {
        throw std::runtime_error("Bindless: vkAllocateDescriptorSets falhou.");
    }
}

void Bindless::shutdown() {
    if (device_ == VK_NULL_HANDLE) return;
    vkDestroyDescriptorPool(device_, pool_, nullptr); // frees the set
    vkDestroyDescriptorSetLayout(device_, layout_, nullptr);
    for (VkSampler s : samplers_) vkDestroySampler(device_, s, nullptr);
    *this = {};
}

uint32_t Bindless::add(VkImageView view) {
    if (count_ >= Capacity) throw std::runtime_error("Bindless: capacidade (1024) esgotada.");
    uint32_t i = count_++;
    set(i, view);
    return i;
}

void Bindless::set(uint32_t index, VkImageView view) {
    VkDescriptorImageInfo ii{VK_NULL_HANDLE, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set_;
    w.dstBinding = 0;
    w.dstArrayElement = index;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(device_, 1, &w, 0, nullptr);
}
