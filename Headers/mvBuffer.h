#ifndef HEADERS_MVBUFFER_H_
#define HEADERS_MVBUFFER_H_

#include <cassert>
#include <cstring>
#include <vulkan/vulkan.hpp>

namespace mv {

  struct Buffer {
    Buffer() {
      buffer = std::make_unique<vk::Buffer>();
      memory = std::make_unique<vk::DeviceMemory>();
    }
    ~Buffer() {
    }

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;

    Buffer(Buffer &&) = default;
    Buffer &operator=(Buffer &&) = default;

    // owns
    std::unique_ptr<vk::Buffer> buffer;
    std::unique_ptr<vk::DeviceMemory> memory;

    // info structures
    vk::DeviceSize size;
    vk::DeviceSize alignment;
    vk::DescriptorBufferInfo descriptor;

    vk::BufferUsageFlags usage_flags;
    vk::MemoryPropertyFlags memory_property_flags;

    // ptr to mapped memory for host visible/coherent buffers
    void *mapped = nullptr;

    void map(const vk::Device &l_dvc, vk::DeviceSize size = VK_WHOLE_SIZE,
             vk::DeviceSize offset = 0);

    void unmap(const vk::Device &l_dvc);

    void bind(const vk::Device &l_dvc, vk::DeviceSize offset = 0);

    void setup_descriptor(vk::DeviceSize size = VK_WHOLE_SIZE, vk::DeviceSize offset = 0);

    void copy_from(void *data, vk::DeviceSize size);

    // TODO
    // flush buffer
    // invalidate buffer

    void destroy(const vk::Device &l_dvc);
  };

}; // namespace mv

#endif