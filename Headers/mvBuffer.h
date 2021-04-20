#ifndef HEADERS_MVBUFFER_H_
#define HEADERS_MVBUFFER_H_

#include <vulkan/vulkan.h>
#include <cassert>
#include <cstring>

namespace mv
{

    struct Buffer
    {
        VkDevice device = nullptr;
        VkBuffer buffer = nullptr;
        VkDeviceMemory memory = nullptr;
        VkDescriptorBufferInfo descriptor = {};

        VkDeviceSize size = 0;
        VkDeviceSize alignment = 0;
        
        void* mapped = nullptr;

        VkBufferUsageFlags usage_flags = 0;
        VkMemoryPropertyFlags memory_property_flags = 0;

        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap(void);

        VkResult bind(VkDeviceSize offset = 0);

        void setup_descriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        void copy_to(void* data, VkDeviceSize size);

        // TODO
        // flush buffer
        // invalidate buffer

        void destroy(void);
    };

};

#endif