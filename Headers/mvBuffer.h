#ifndef HEADERS_MVBUFFER_H_
#define HEADERS_MVBUFFER_H_

#include <vulkan/vulkan.hpp>
#include <cassert>
#include <cstring>

namespace mv
{

    struct Buffer
    {
        Buffer(){
            buffer = std::make_shared<vk::Buffer>();
            memory = std::make_shared<vk::DeviceMemory>();
        }
        ~Buffer(){}
        // owns
        std::shared_ptr<vk::Buffer> buffer;
        std::shared_ptr<vk::DeviceMemory> memory;

        // references
        std::weak_ptr<vk::Device> logical_device;

        // info structures
        vk::DeviceSize size;
        vk::DeviceSize alignment;
        vk::DescriptorBufferInfo descriptor;

        vk::BufferUsageFlags usage_flags;
        vk::MemoryPropertyFlags memory_property_flags;
        
        void map(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);
        void unmap(void);
        void* mapped = nullptr;

        void bind(vk::DeviceSize offset = 0);

        void setup_descriptor(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

        void copy_from(void* data, vk::DeviceSize size);

        // TODO
        // flush buffer
        // invalidate buffer

        void destroy(void);
    };

};

#endif