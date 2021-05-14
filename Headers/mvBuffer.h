#ifndef HEADERS_MVBUFFER_H_
#define HEADERS_MVBUFFER_H_

#include <cassert>
#include <cstring>
#include <vulkan/vulkan.hpp>

#include "mvDevice.h"

namespace mv
{

    // Forward decl
    struct Device;
    struct Buffer
    {
        Buffer()
        {
            buffer = std::make_unique<vk::Buffer>();
            memory = std::make_unique<vk::DeviceMemory>();
        }
        ~Buffer()
        {
        }

        Buffer(const Buffer &) = delete;
        Buffer &operator=(const Buffer &) = delete;

        Buffer(Buffer &&) = default;
        Buffer &operator=(Buffer &&) = default;

        // owns
        std::unique_ptr<vk::Buffer> buffer;
        std::unique_ptr<vk::DeviceMemory> memory;

        // info structures
        // clang-format off
        vk::DeviceSize            size;
        vk::DeviceSize            alignment;
        vk::DescriptorBufferInfo  descriptor;
        vk::BufferUsageFlags      usageFlags;
        vk::MemoryPropertyFlags   memoryPropertyFlags;
        void *                    mapped = nullptr;
        // clang-format on

        void map(const mv::Device &p_MvDevice, vk::DeviceSize p_MemorySize = VK_WHOLE_SIZE,
                 vk::DeviceSize p_MemoryStartOffset = 0);

        void unmap(const mv::Device &p_MvDevice);

        void bind(const mv::Device &p_MvDevice, vk::DeviceSize p_MemoryOffset = 0);

        void setupDescriptor(vk::DeviceSize p_MemorySize = VK_WHOLE_SIZE, vk::DeviceSize p_MemoryOffset = 0);

        void copyFrom(void *p_SourceData, vk::DeviceSize p_MemoryCopySize);

        // TODO
        // flush buffer
        // invalidate buffer

        void destroy(const mv::Device &p_MvDevice);
    };

}; // namespace mv

#endif