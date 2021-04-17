#ifndef HEADERS_MVDEVICE_H_
#define HEADERS_MVDEVICE_H_

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <algorithm>
#include <stdexcept>

#include "mvInit.h"
#include "mvBuffer.h"

namespace mv
{
    struct Device
    {
        explicit Device(VkPhysicalDevice physicaldevice);
        ~Device();

        VkPhysicalDevice physicalDevice = nullptr;
        VkDevice device = nullptr;
        VkQueue graphicsQueue = nullptr;

        VkPhysicalDeviceFeatures enabledFeatures = {};
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceMemoryProperties memoryProperties = {};

        // Supported device level extensions
        std::vector<std::string> supportedExtensions;
        std::vector<VkQueueFamilyProperties> queueFamilyProperties;

        VkCommandPool m_CommandPool = nullptr;

        struct
        {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queueFamilyIndices;

        VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> requestedExtensions);
        VkCommandPool createCommandPool(uint32_t queueIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr) const;

        uint32_t getQueueFamilyIndex(VkQueueFlags queueFlag);
        bool extensionSupported(std::string ext);
        VkFormat getSupportedDepthFormat(VkPhysicalDevice physicalDevice);

        VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);
        VkResult createBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, mv::Buffer *buffer, VkDeviceSize size, void *data = nullptr);
    };
};

#endif