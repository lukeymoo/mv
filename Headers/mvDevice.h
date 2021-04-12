#ifndef HEADERS_MVDEVICE_H_
#define HEADERS_MVDEVICE_H_

#include <vulkan/vulkan.h>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <algorithm>
#include <stdexcept>

namespace mv
{
    struct Device
    {
        explicit Device(VkPhysicalDevice physicaldevice);
        ~Device();

        VkPhysicalDevice m_PhysicalDevice = nullptr;
        VkDevice m_Device = nullptr;
        VkQueue m_GraphicsQueue = nullptr;

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

        uint32_t getQueueFamilyIndex(VkQueueFlags queueFlag);
        bool extensionSupported(std::string ext);
        VkFormat getSupportedDepthFormat(VkPhysicalDevice physicalDevice);
    };
};

#endif