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
        explicit Device(VkPhysicalDevice physical_device);
        ~Device();

        VkPhysicalDevice physical_device = nullptr;
        VkDevice device = nullptr;
        VkQueue graphics_queue = nullptr;

        VkPhysicalDeviceFeatures enabled_features = {};
        VkPhysicalDeviceProperties properties = {};
        VkPhysicalDeviceMemoryProperties memory_properties = {};

        // Supported device level extensions
        std::vector<std::string> supported_extensions;
        std::vector<VkQueueFamilyProperties> queue_family_properties;

        VkCommandPool m_command_pool = nullptr;

        struct
        {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queue_family_indices;

        VkResult create_logical_device(VkPhysicalDeviceFeatures enabled_features, std::vector<const char *> requested_extensions);
        VkCommandPool create_command_pool(uint32_t queue_index, VkCommandPoolCreateFlags create_flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        uint32_t get_memory_type(uint32_t type_bits, VkMemoryPropertyFlags properties, VkBool32 *mem_type_found = nullptr) const;

        uint32_t get_queue_family_index(VkQueueFlags queue_flag);
        bool extension_supported(std::string ext);
        VkFormat get_supported_depth_format(VkPhysicalDevice physical_device);

        VkResult create_buffer(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_property_flags, VkDeviceSize size, VkBuffer *buffer, VkDeviceMemory *memory, void *data = nullptr);
        VkResult create_buffer(VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags memory_property_flags, mv::Buffer *buffer, VkDeviceSize size, void *data = nullptr);
    };
};

#endif