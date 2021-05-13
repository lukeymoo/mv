#include "mvBuffer.h"

// Maps vulkan buffer/memory to member variable void* mapped
void mv::Buffer::map(const mv::Device &p_MvDevice, vk::DeviceSize p_MemorySize, vk::DeviceSize p_MemoryOffset)
{
    mapped = p_MvDevice.logicalDevice->mapMemory(*memory, p_MemoryOffset, p_MemorySize);
    if (!mapped)
        throw std::runtime_error("Failed to map memory :: buffer handler");
    return;
}

// Unmaps void* mapped from vulkan buffer/memory
void mv::Buffer::unmap(const mv::Device &p_MvDevice)
{
    if (mapped)
    {
        p_MvDevice.logicalDevice->unmapMemory(*memory);
        mapped = nullptr;
    }
}

// Bind allocated memory to buffer
void mv::Buffer::bind(const mv::Device &p_MvDevice, vk::DeviceSize p_MemoryOffset)
{
    // TODO
    // Implement check against memory size & parameter memory offset
    // Should do some bounds checking instead of letting Vulkan throw for us
    p_MvDevice.logicalDevice->bindBufferMemory(*buffer, *memory, p_MemoryOffset);
    return;
}

// Configure default descriptor values
void mv::Buffer::setupDescriptor(vk::DeviceSize p_MemorySize, vk::DeviceSize p_MemoryOffset)
{
    descriptor.buffer = *buffer;
    descriptor.range = p_MemorySize;
    descriptor.offset = p_MemoryOffset;
    return;
}

// Copy buffer to another mapped buffer pointed to via function param void* data
void mv::Buffer::copyFrom(void *p_SrcData, vk::DeviceSize p_MemoryCopySize)
{
    // TODO
    // Should do bounds checking to ensure we're not attempting to copy data from
    // Source that is larger than our own
    if (!mapped)
        throw std::runtime_error("Requested to copy data to buffer but the buffer was never mapped :: buffer handler");

    memcpy(mapped, p_SrcData, p_MemoryCopySize);

    return;
}

// Unmaps void* mapped if it was previously mapped to vulkan buffer/memory
// Destroys buffer and frees allocated memory
void mv::Buffer::destroy(const mv::Device &p_MvDevice)
{

    unmap(p_MvDevice);

    if (buffer)
    {
        p_MvDevice.logicalDevice->destroyBuffer(*buffer);
        buffer.reset();
    }

    if (memory)
    {
        p_MvDevice.logicalDevice->freeMemory(*memory);
        memory.reset();
    }
}