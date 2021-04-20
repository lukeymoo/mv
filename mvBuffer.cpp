#include "mvBuffer.h"

// Maps vulkan buffer/memory to member variable void* mapped
VkResult mv::Buffer::map(VkDeviceSize psize, VkDeviceSize poffset)
{
    return vkMapMemory(device, memory, poffset, psize, 0, &mapped);
}

// Unmaps void* mapped from vulkan buffer/memory
void mv::Buffer::unmap(void)
{
    if (mapped)
    {
        vkUnmapMemory(device, memory);
        mapped = nullptr;
    }
}

// Bind allocated memory to buffer
VkResult mv::Buffer::bind(VkDeviceSize poffset)
{
   return vkBindBufferMemory(device, buffer, memory, poffset);
}

// Configure default descriptor values
void mv::Buffer::setup_descriptor(VkDeviceSize psize, VkDeviceSize poffset)
{
    descriptor.buffer = buffer;
    descriptor.range = psize;
    descriptor.offset = poffset;
}

// Copy buffer to another mapped buffer pointed to via function param void* data
void mv::Buffer::copy_to(void *data, VkDeviceSize size)
{
    assert(mapped);
    memcpy(mapped, data, size);
}

// Unmaps void* mapped if it was previously mapped to vulkan buffer/memory
// Destroys buffer and frees allocated memory
void mv::Buffer::destroy(void)
{
    unmap();
    if (buffer)
    {
        vkDestroyBuffer(device, buffer, nullptr);
    }
    if (memory)
    {
        vkFreeMemory(device, memory, nullptr);
    }
}