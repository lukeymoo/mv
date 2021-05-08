#include "mvBuffer.h"

// Maps vulkan buffer/memory to member variable void* mapped
void mv::Buffer::map(vk::DeviceSize psize, vk::DeviceSize poffset)
{
    std::shared_ptr<vk::Device> l_dvc = std::make_shared<vk::Device>(logical_device);

    mapped = l_dvc->mapMemory(*memory, poffset, psize);
    if (!mapped)
        throw std::runtime_error("Failed to map memory :: buffer handler");
    return;
}

// Unmaps void* mapped from vulkan buffer/memory
void mv::Buffer::unmap(void)
{
    if (mapped)
    {
        std::shared_ptr<vk::Device> l_dvc = std::make_shared<vk::Device>(logical_device);
        l_dvc->unmapMemory(*memory);
        mapped = nullptr;
    }
}

// Bind allocated memory to buffer
void mv::Buffer::bind(vk::DeviceSize poffset)
{
    std::shared_ptr<vk::Device> l_dvc = std::make_shared<vk::Device>(logical_device);

    l_dvc->bindBufferMemory(*buffer, *memory, poffset);

    return;
}

// Configure default descriptor values
void mv::Buffer::setup_descriptor(vk::DeviceSize psize, vk::DeviceSize poffset)
{
    descriptor.buffer = *buffer;
    descriptor.range = psize;
    descriptor.offset = poffset;
    return;
}

// Copy buffer to another mapped buffer pointed to via function param void* data
void mv::Buffer::copy_from(void *data, vk::DeviceSize size)
{
    if (!mapped)
        throw std::runtime_error("Requested to copy data to buffer but the buffer was never mapped :: buffer handler");

    memcpy(mapped, data, size);

    return;
}

// Unmaps void* mapped if it was previously mapped to vulkan buffer/memory
// Destroys buffer and frees allocated memory
void mv::Buffer::destroy(void)
{
    std::shared_ptr<vk::Device> l_dvc = std::make_shared<vk::Device>(logical_device);

    unmap();
    if (buffer)
    {
        l_dvc->destroyBuffer(*buffer);
        buffer.reset();
    }
    if (memory)
    {
        l_dvc->freeMemory(*memory);
        memory.reset();
    }
}