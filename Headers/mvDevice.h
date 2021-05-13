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
    // Forward decl
    struct Buffer;
    struct Device
    {
        Device(const vk::PhysicalDevice &p_PhysicalDevice, std::vector<std::string> &p_RequestedDeviceExtensions);
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
        std::vector<std::string>                          requestedPhysicalDeviceExtensions;
        vk::PhysicalDeviceFeatures                        physicalFeatures;
        vk::PhysicalDeviceFeatures2                       physicalFeatures2;
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedFeatures;
        vk::PhysicalDeviceProperties                      physicalProperties;
        vk::PhysicalDeviceMemoryProperties                physicalMemoryProperties;
        std::vector<vk::ExtensionProperties>              physicalDeviceExtensions;
        std::vector<vk::QueueFamilyProperties>            queueFamilyProperties;
        // clang-format on

        struct
        {
            uint32_t graphics = UINT32_MAX;
            uint32_t compute = UINT32_MAX;
            uint32_t transfer = UINT32_MAX;
        } queueIdx;

        void createLogicalDevice(const vk::PhysicalDevice &p_PhysicalDevice);
        vk::CommandPool createCommandPool(
            uint32_t p_QueueIndex,
            vk::CommandPoolCreateFlags p_CreateFlags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);

        uint32_t getMemoryType(uint32_t p_TypeBits, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                               vk::Bool32 *p_IsMemoryTypeFound = nullptr) const;

        uint32_t getQueueFamilyIndex(vk::QueueFlagBits p_QueueFlagBit) const;

        vk::Format getSupportedDepthFormat(const vk::PhysicalDevice &p_PhysicalDevice);

        // create buffer with Vulkan objects
        void createBuffer(vk::BufferUsageFlags p_BufferUsageFlags, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                          vk::DeviceSize p_DeviceSize, vk::Buffer *p_VkBuffer, vk::DeviceMemory *p_DeviceMemory,
                          void *p_InitialData = nullptr) const;

        // create buffer with custom mv::Buffer interface
        void createBuffer(vk::BufferUsageFlags p_BufferUsageFlags, vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                          mv::Buffer *p_MvBuffer, vk::DeviceSize p_DeviceSize, void *p_InitialData = nullptr) const;
    };
}; // namespace mv

#endif