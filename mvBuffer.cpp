#include "mvBuffer.h"

// Maps vulkan buffer/memory to member variable void* mapped
void
MvBuffer::map (const vk::Device &p_LogicalDevice,
               vk::DeviceSize p_MemorySize,
               vk::DeviceSize p_MemoryOffset)
{
  if (!canMap)
    {
      std::cout << "MvBuffer is not capable of being mapped!\n";
      return;
    }
  mapped = p_LogicalDevice.mapMemory (memory, p_MemoryOffset, p_MemorySize);
  if (!mapped)
    throw std::runtime_error ("Failed to map memory :: buffer handler");
  return;
}

// Unmaps void* mapped from vulkan buffer/memory
void
MvBuffer::unmap (const vk::Device &p_LogicalDevice)
{
  if (mapped)
    {
      p_LogicalDevice.unmapMemory (memory);
      mapped = nullptr;
    }
}

// Bind allocated memory to buffer
void
MvBuffer::bind (const vk::Device &p_LogicalDevice, vk::DeviceSize p_MemoryOffset)
{
  // TODO
  // Implement check against memory size & parameter memory offset
  // Should do some bounds checking instead of letting Vulkan throw for us
  p_LogicalDevice.bindBufferMemory (buffer, memory, p_MemoryOffset);
  return;
}

void
MvBuffer::allocate (Allocator &p_DescriptorAllocator,
                    vk::DescriptorSetLayout &p_Layout,
                    vk::DescriptorType p_DescriptorType)
{
  p_DescriptorAllocator.allocateSet (p_Layout, descriptor);
  p_DescriptorAllocator.updateSet (bufferInfo, descriptor, p_DescriptorType, 0);
  return;
}

// Configure default descriptor values
void
MvBuffer::setupBufferInfo (vk::DeviceSize p_MemorySize, vk::DeviceSize p_MemoryOffset)
{
  bufferInfo.buffer = buffer;
  bufferInfo.range = p_MemorySize;
  bufferInfo.offset = p_MemoryOffset;
  return;
}

// Copy buffer to another mapped buffer pointed to via function param void* data
void
MvBuffer::copyFrom (void *p_SrcData, vk::DeviceSize p_MemoryCopySize)
{
  if (!canMap)
    {
      std::cout << "Cannot copy " << static_cast<uint32_t> (p_MemoryCopySize)
                << " bytes from location [" << p_SrcData
                << "] this MvBuffer is not capable of being mapped\n";
      return;
    }
  // TODO
  // Should do bounds checking to ensure we're not attempting to copy data
  // from Source that is larger than our own
  if (!mapped)
    throw std::runtime_error ("Requested to copy data to buffer but the "
                              "buffer was never mapped :: buffer handler");

  memcpy (mapped, p_SrcData, p_MemoryCopySize);

  return;
}

// Unmaps void* mapped if it was previously mapped to vulkan buffer/memory
// Destroys buffer and frees allocated memory
void
MvBuffer::destroy (const vk::Device &p_LogicalDevice)
{
  unmap (p_LogicalDevice);

  if (buffer)
    {
      p_LogicalDevice.destroyBuffer (buffer);
    }

  if (memory)
    {
      p_LogicalDevice.freeMemory (memory);
    }
}