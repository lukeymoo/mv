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

        VkBufferUsageFlags usageFlags = 0;
        VkMemoryPropertyFlags memoryPropertyFlags = 0;

        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap(void);

        VkResult bind(VkDeviceSize offset = 0);

        void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        void copyTo(void* data, VkDeviceSize size);

        // TODO
        // flush buffer
        // invalidate buffer

        void destroy(void);
    };

};

#endif