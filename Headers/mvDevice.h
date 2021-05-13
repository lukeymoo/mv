#ifndef HEADERS_MVDEVICE_H_
#define HEADERS_MVDEVICE_H_

#include <algorithm>
#include <cassert>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include <iostream>

#include "mvBuffer.h"

namespace mv
{
    struct Buffer;
    struct Device
    {
        Device(const vk::PhysicalDevice &p_dvc, std::vector<std::string> &requested_device_exts);
        ~Device();

        // do not allow assignment
        Device &operator=(const Device &) = delete;

        // allow move
        Device(Device &&) = default;
        Device &operator=(Device &&) = default;

        // owns
        std::unique_ptr<vk::Device> logicalDevice;
        std::unique_ptr<vk::Queue> graphicsQueue;
        std::unique_ptr<vk::CommandPool> commandPool;

        // info structures
        // clang-format off
        std::vector<std::string>                          requestedPhysicalDeviceExts;
        vk::PhysicalDeviceFeatures                        physicalFeatures;
        vk::PhysicalDeviceFeatures2                       physicalFeatures2;
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedFeatures;
        vk::PhysicalDeviceProperties                      physicalProperties;
        vk::PhysicalDeviceMemoryProperties                physicalMemoryProperties;
        std::vector<vk::ExtensionProperties>              physicalDeviceExts;
        std::vector<vk::QueueFamilyProperties>            queueFamilyProperties;
        // clang-format on

        struct
        {
            uint32_t graphics = UINT32_MAX;
            uint32_t compute = UINT32_MAX;
            uint32_t transfer = UINT32_MAX;
        } queueIdx;

        void createLogicalDevice(const vk::PhysicalDevice &p_dvc);
        vk::CommandPool createCommandPool(uint32_t queue_index, vk::CommandPoolCreateFlags create_flags =
                                                                    vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        uint32_t getMemoryType(uint32_t type_bits, vk::MemoryPropertyFlags properties,
                               vk::Bool32 *mem_type_found = nullptr) const;

        uint32_t getQueueFamilyIndex(vk::QueueFlagBits queue_flag_bit) const;

        vk::Format getSupportedDepthFormat(const vk::PhysicalDevice &p_dvc);

        // create buffer with Vulkan objects
        void createBuffer(vk::BufferUsageFlags usage_flags, vk::MemoryPropertyFlags memory_property_flags,
                          vk::DeviceSize size, vk::Buffer *buffer, vk::DeviceMemory *memory,
                          void *data = nullptr) const;

        // create buffer with custom mv::Buffer interface
        void createBuffer(vk::BufferUsageFlags usage_flags, vk::MemoryPropertyFlags memory_property_flags,
                          mv::Buffer *buffer, vk::DeviceSize size, void *data = nullptr) const;
    };
}; // namespace mv

#endif