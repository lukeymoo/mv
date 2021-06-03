#pragma once

#include <cassert>
#include <cstring>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "mvAllocator.h"

class Allocator;
class MvBuffer
{
public:
  MvBuffer () {}
  ~MvBuffer () {}

  bool canMap = false;

  vk::Buffer buffer;
  vk::DeviceMemory memory;

  // info structures
  // clang-format off
    vk::DeviceSize            size;
    vk::DeviceSize            alignment;
    vk::DescriptorSet         descriptor;
    vk::DescriptorBufferInfo  bufferInfo;
    vk::BufferUsageFlags      usageFlags;
    vk::MemoryPropertyFlags   memoryPropertyFlags;
    void *                    mapped = nullptr;
  // clang-format on

  // Allocates and updates descriptor set for bufferInfo
  void allocate (Allocator &p_DescriptorAllocator,
                 vk::DescriptorSetLayout &p_Layout,
                 vk::DescriptorType p_DescriptorType);

  void map (const vk::Device &p_LogicalDevice,
            vk::DeviceSize p_MemoryStartOffset = 0,
            vk::DeviceSize p_MemorySize = VK_WHOLE_SIZE);

  void unmap (const vk::Device &p_LogicalDevice);

  void bind (const vk::Device &p_LogicalDevice, vk::DeviceSize p_MemoryOffset = 0);

  void setupBufferInfo (vk::DeviceSize p_MemorySize = VK_WHOLE_SIZE,
                        vk::DeviceSize p_MemoryOffset = 0);

  void copyFrom (void *p_SourceData, vk::DeviceSize p_MemoryCopySize);

  // TODO
  // flush buffer
  // invalidate buffer

  void destroy (const vk::Device &p_LogicalDevice);
};

struct UniformObject
{
  alignas (16) glm::mat4 matrix = glm::mat4 (1.0f);
  MvBuffer mvBuffer;
};