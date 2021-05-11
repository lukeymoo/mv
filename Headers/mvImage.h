#ifndef HEADERS_MVIMAGE_H_
#define HEADERS_MVIMAGE_H_

#include <stdexcept>
#include <vulkan/vulkan.hpp>

#include "mvDevice.h"
#include "stb_image.h"

#include <iostream>

namespace mv {
  struct Image {
    // delete copy
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;

    // allow move
    Image(Image &&) = default;
    Image &operator=(Image &&) = default;

    Image(void) {
    }
    ~Image() {
    } // image is cleaned up in engine

    // owns
    std::unique_ptr<vk::Image> image;
    std::unique_ptr<vk::ImageView> image_view;
    std::unique_ptr<vk::Sampler> sampler;
    std::unique_ptr<vk::DeviceMemory> memory;

    // info structs
    vk::DescriptorImageInfo descriptor;
    vk::MemoryRequirements memory_requirements;

    struct ImageCreateInfo {
      ImageCreateInfo() {
      }
      ~ImageCreateInfo() {
      }
      vk::ImageUsageFlags usage;
      vk::MemoryPropertyFlags memory_properties;
      vk::Format format = vk::Format::eR8G8B8A8Srgb;
      vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    };

    void create(const mv::Device &m_dvc, ImageCreateInfo &c_info, std::string image_filename) {

      if (!m_dvc.command_pool)
        throw std::runtime_error(
            "command pool not initialized in mv device handler:: image handler");

      int width;
      int height;
      int channels;

      // load image
      stbi_uc *raw_image =
          stbi_load(image_filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
      vk::DeviceSize image_size = width * height * 4;

      if (!raw_image)
        throw std::runtime_error("Failed to load image => " + image_filename);

      vk::Buffer staging_buffer;
      vk::DeviceMemory staging_memory;
      void *staging_mapped = nullptr;

      // create staging buffer
      create_staging_buffer(m_dvc, image_size, staging_buffer, staging_memory);

      staging_mapped = m_dvc.logical_device->mapMemory(staging_memory, 0, image_size);

      // copy raw image to staging memory
      memcpy(staging_mapped, raw_image, static_cast<size_t>(image_size));

      m_dvc.logical_device->unmapMemory(staging_memory);

      // free stb image
      stbi_image_free(raw_image);

      // create vulkan image that will store our pixel data
      vk::ImageCreateInfo image_info;
      image_info.imageType = vk::ImageType::e2D;
      image_info.format = c_info.format;
      image_info.extent.width = width;
      image_info.extent.height = height;
      image_info.extent.depth = 1;
      image_info.mipLevels = 1;
      image_info.arrayLayers = 1;
      image_info.samples = vk::SampleCountFlagBits::e1;
      image_info.tiling = c_info.tiling;
      image_info.usage =
          c_info.usage; // VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
      image_info.sharingMode = vk::SharingMode::eExclusive;
      image_info.initialLayout = vk::ImageLayout::eUndefined;

      image = std::make_unique<vk::Image>(m_dvc.logical_device->createImage(image_info));

      memory_requirements = m_dvc.logical_device->getImageMemoryRequirements(*image);

      vk::MemoryAllocateInfo alloc_info;
      alloc_info.allocationSize = memory_requirements.size;
      alloc_info.memoryTypeIndex =
          m_dvc.get_memory_type(memory_requirements.memoryTypeBits, c_info.memory_properties);

      // allocate image memory
      memory = std::make_unique<vk::DeviceMemory>(m_dvc.logical_device->allocateMemory(alloc_info));

      m_dvc.logical_device->bindImageMemory(*image, *memory, 0);

      // transition image to transfer dst
      transition_image_layout(m_dvc, image.get(), vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eTransferDstOptimal);

      // copy staging buffer to image
      vk::CommandBuffer command_buffer = begin_command_buffer(m_dvc);

      vk::BufferImageCopy region;
      region.bufferOffset = 0;
      region.bufferRowLength = 0;
      region.bufferImageHeight = 0;
      region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
      region.imageSubresource.mipLevel = 0;
      region.imageSubresource.baseArrayLayer = 0;
      region.imageSubresource.layerCount = 1;
      region.imageOffset = vk::Offset3D{0, 0, 0};
      region.imageExtent =
          vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

      command_buffer.copyBufferToImage(staging_buffer, *image, vk::ImageLayout::eTransferDstOptimal,
                                       region);

      end_command_buffer(m_dvc, command_buffer);

      // transition image to shader read only
      transition_image_layout(m_dvc, image.get(), vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);

      // create view into image
      vk::ImageViewCreateInfo view_info;
      view_info.image = *image;
      view_info.viewType = vk::ImageViewType::e2D;
      view_info.format = c_info.format;
      view_info.components = {vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG,
                              vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA};
      view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      view_info.subresourceRange.baseMipLevel = 0;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.baseArrayLayer = 0;
      view_info.subresourceRange.layerCount = 1;

      // create image view
      image_view =
          std::make_unique<vk::ImageView>(m_dvc.logical_device->createImageView(view_info));

      // create image sampler
      vk::SamplerCreateInfo sampler_info;
      sampler_info.magFilter = vk::Filter::eLinear;
      sampler_info.minFilter = vk::Filter::eLinear;
      sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
      sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
      sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
      sampler_info.anisotropyEnable = VK_TRUE;
      sampler_info.maxAnisotropy = m_dvc.physical_properties.limits.maxSamplerAnisotropy;
      sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
      sampler_info.unnormalizedCoordinates = VK_FALSE;
      sampler_info.compareEnable = VK_FALSE;
      sampler_info.compareOp = vk::CompareOp::eAlways;
      sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;

      // create sampler
      sampler = std::make_unique<vk::Sampler>(m_dvc.logical_device->createSampler(sampler_info));

      // cleanup staging resources
      m_dvc.logical_device->destroyBuffer(staging_buffer);
      m_dvc.logical_device->freeMemory(staging_memory);

      // setup the descriptor
      descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
      descriptor.imageView = *image_view;
      descriptor.sampler = *sampler;

      std::cout << "Loaded model texture" << std::endl;

      return;
    }

    void create_staging_buffer(const mv::Device &m_dvc, vk::DeviceSize &size,
                               vk::Buffer &staging_buffer, vk::DeviceMemory &staging_memory) {

      vk::MemoryRequirements staging_req;

      vk::BufferCreateInfo buffer_info;
      buffer_info.size = size;
      buffer_info.sharingMode = vk::SharingMode::eExclusive;
      buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;

      // create staging buffer
      staging_buffer = m_dvc.logical_device->createBuffer(buffer_info);

      // get memory requirements
      staging_req = m_dvc.logical_device->getBufferMemoryRequirements(staging_buffer);

      vk::MemoryAllocateInfo alloc_info;
      alloc_info.allocationSize = staging_req.size;
      alloc_info.memoryTypeIndex = m_dvc.get_memory_type(
          staging_req.memoryTypeBits,
          vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

      // allocate staging memory
      staging_memory = m_dvc.logical_device->allocateMemory(alloc_info);

      // bind staging buffer & memory
      m_dvc.logical_device->bindBufferMemory(staging_buffer, staging_memory, 0);
      return;
    }

    void transition_image_layout(const mv::Device &m_dvc, vk::Image *image,
                                 vk::ImageLayout initial_layout, vk::ImageLayout final_layout) {
      vk::CommandBuffer command_buffer = begin_command_buffer(m_dvc);

      vk::ImageMemoryBarrier barrier;
      barrier.oldLayout = initial_layout;
      barrier.newLayout = final_layout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = *image;
      barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
      barrier.subresourceRange.baseMipLevel = 0;
      barrier.subresourceRange.levelCount = 1;
      barrier.subresourceRange.baseArrayLayer = 0;
      barrier.subresourceRange.layerCount = 1;

      vk::PipelineStageFlags source_stage;
      vk::PipelineStageFlags destination_stage;

      if (initial_layout == vk::ImageLayout::eUndefined &&
          final_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        source_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        destination_stage = vk::PipelineStageFlagBits::eTransfer;
      } else if (initial_layout == vk::ImageLayout::eTransferDstOptimal &&
                 final_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        source_stage = vk::PipelineStageFlagBits::eTransfer;
        destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
      } else {
        throw std::runtime_error("Layout transition requested is unsupported");
      }

      command_buffer.pipelineBarrier(source_stage, destination_stage, vk::DependencyFlags(),
                                     nullptr, nullptr, barrier);

      end_command_buffer(m_dvc, command_buffer);
      return;
    }

    vk::CommandBuffer begin_command_buffer(const mv::Device &m_dvc) {

      if (!m_dvc.command_pool)
        throw std::runtime_error(
            "Command pool not initialized in mv device handler :: image handler");

      vk::CommandBufferAllocateInfo alloc_info;
      alloc_info.level = vk::CommandBufferLevel::ePrimary;
      alloc_info.commandPool = *m_dvc.command_pool;
      alloc_info.commandBufferCount = 1;

      // create command buffer
      std::vector<vk::CommandBuffer> cmd_buffers =
          m_dvc.logical_device->allocateCommandBuffers(alloc_info);

      if (cmd_buffers.size() < 1)
        throw std::runtime_error("Failed to allocate command buffers :: image handler");

      // destroy other command buffers if more than 1 created
      // shouldn't happen
      if (cmd_buffers.size() > 1) {
        std::vector<vk::CommandBuffer> to_destroy;
        for (auto &buf : cmd_buffers) {
          if (&buf - &cmd_buffers[0] > 0) {
            to_destroy.push_back(buf);
          }
        }
        m_dvc.logical_device->freeCommandBuffers(*m_dvc.command_pool, to_destroy);
      }

      vk::CommandBufferBeginInfo begin_info;
      begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

      cmd_buffers.at(0).begin(begin_info);

      return cmd_buffers.at(0);
    }

    void end_command_buffer(const mv::Device &m_dvc, vk::CommandBuffer command_buffer) {

      if (!m_dvc.command_pool)
        throw std::runtime_error("Command pool invalid ending commands :: image handler");

      if (!m_dvc.graphics_queue)
        throw std::runtime_error(
            "Graphics queue reference invalid ending commands :: image handler");

      command_buffer.end();

      vk::SubmitInfo submit_info;
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers = &command_buffer;

      m_dvc.graphics_queue->submit(submit_info);
      m_dvc.graphics_queue->waitIdle();

      m_dvc.logical_device->freeCommandBuffers(*m_dvc.command_pool, command_buffer);
      return;
    }

    void destroy(const mv::Device &m_dvc) {

      if (sampler) {
        m_dvc.logical_device->destroySampler(*sampler);
        sampler.reset();
      }
      if (image_view) {
        m_dvc.logical_device->destroyImageView(*image_view);
        image_view.reset();
      }
      if (image) {
        m_dvc.logical_device->destroyImage(*image);
        image.reset();
      }
      if (memory) {
        m_dvc.logical_device->freeMemory(*memory);
        memory.reset();
      }
      return;
    }
  };
}; // namespace mv

#endif