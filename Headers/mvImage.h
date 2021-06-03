#pragma once

#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "stb_image.h"

#include "mvEngine.h"

#include <iostream>

class Image
{
public:
  Image (void);
  ~Image (); // Image cleaned up in engine

  // initialized in call to create()
  const vk::PhysicalDevice *physicalDevice = nullptr;
  const vk::Device *logicalDevice = nullptr;

  vk::Image image;
  vk::ImageView imageView;
  vk::Sampler sampler;
  vk::DeviceMemory memory;

  // info structs
  vk::DescriptorImageInfo descriptor;
  vk::MemoryRequirements memoryRequirements;

  struct ImageCreateInfo
  {
    // clang-format off
            vk::ImageUsageFlags     usage;
            vk::MemoryPropertyFlags memoryProperties;
            vk::Format              format = vk::Format::eR8G8B8A8Srgb;
            vk::ImageTiling         tiling = vk::ImageTiling::eOptimal;
            vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    // clang-format on
  };

  // Use when creating depth stencil
  void create (const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice,
               const size_t p_ImageWidth,
               const size_t p_ImageHeight,
               const vk::Format p_DepthFormat,
               const vk::SampleCountFlagBits p_SampleCount);

  // Create image and preload with data from file
  void create (const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice,
               const vk::CommandPool &p_CommandPool,
               const vk::Queue &p_GraphicsQueue,
               ImageCreateInfo &p_ImageCreateInfo,
               std::string p_ImageFilename);

  // MSAA image creation
  void create (const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice,
               const size_t p_ImageWidth,
               const size_t p_ImageHeight,
               const ImageCreateInfo &p_ImageCreateInfo);

  void createStagingBuffer (const vk::PhysicalDevice &p_PhysicalDevice,
                            const vk::Device &p_LogicalDevice,
                            vk::DeviceSize &p_BufferSize,
                            vk::Buffer &p_StagingBuffer,
                            vk::DeviceMemory &p_StagingMemory);

  void transitionImageLayout (const vk::Device &p_LogicalDevice,
                              const vk::CommandPool &p_CommandPool,
                              const vk::Queue &p_GraphicsQueue,
                              vk::Image *p_TargetImage,
                              vk::ImageLayout p_StartingLayout,
                              vk::ImageLayout p_EndingLayout);

  vk::CommandBuffer beginCommandBuffer (const vk::Device &p_LogicalDevice,
                                        const vk::CommandPool &p_CommandPool);

  void endCommandBuffer (const vk::Device &p_LogicalDevice,
                         const vk::CommandPool &p_CommandPool,
                         const vk::Queue &p_GraphicsQueue,
                         vk::CommandBuffer p_CommandBuffer);

  void destroy (void);

private:
  uint32_t getMemoryType (const vk::PhysicalDevice &p_PhysicalDevice,
                          uint32_t p_MemoryTypeBits,
                          const vk::MemoryPropertyFlags p_MemoryProperties,
                          vk::Bool32 *p_IsMemoryTypeFound = nullptr) const;
};