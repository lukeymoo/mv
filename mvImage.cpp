#define STB_IMAGE_IMPLEMENTATION
#include "mvImage.h"

Image::Image () { return; }

Image::~Image () { return; }

void
Image::create (Engine *p_Engine, ImageCreateInfo &p_ImageCreateInfo,
               std::string p_ImageFilename)
{
  if (!p_Engine)
    throw std::runtime_error ("Invalid engine handle passed to image");
  if (!p_Engine->commandPool)
    throw std::runtime_error ("command pool not initialized in mv device "
                              "handler:: image handler");

  engine = p_Engine;

  int width = 0;
  int height = 0;
  int channels = 0;

  // load image
  stbi_uc *rawImage = stbi_load (p_ImageFilename.c_str (), &width, &height,
                                 &channels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = width * height * 4;

  if (!rawImage)
    throw std::runtime_error ("Failed to load image => " + p_ImageFilename);

  vk::Buffer stagingBuffer;
  vk::DeviceMemory stagingMemory;
  void *stagingMapped = nullptr;

  // create staging buffer
  createStagingBuffer (imageSize, stagingBuffer, stagingMemory);

  stagingMapped
      = p_Engine->logicalDevice.mapMemory (stagingMemory, 0, imageSize);

  // copy raw image to staging memory
  memcpy (stagingMapped, rawImage, static_cast<size_t> (imageSize));

  p_Engine->logicalDevice.unmapMemory (stagingMemory);

  // free stb image
  stbi_image_free (rawImage);

  // In case I forgot to add TransferDst
  if (!(p_ImageCreateInfo.usage & vk::ImageUsageFlagBits::eTransferDst))
    {
      p_ImageCreateInfo.usage |= vk::ImageUsageFlagBits::eTransferDst;
    }

  // create vulkan image that will store our pixel data
  vk::ImageCreateInfo imageInfo;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.format = p_ImageCreateInfo.format;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = vk::SampleCountFlagBits::e1;
  imageInfo.tiling = p_ImageCreateInfo.tiling;
  imageInfo.usage = p_ImageCreateInfo.usage; // VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                             // | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;

  image = p_Engine->logicalDevice.createImage (imageInfo);

  memoryRequirements
      = p_Engine->logicalDevice.getImageMemoryRequirements (image);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = memoryRequirements.size;
  allocInfo.memoryTypeIndex = p_Engine->getMemoryType (
      memoryRequirements.memoryTypeBits, p_ImageCreateInfo.memoryProperties);

  // allocate image memory
  memory = p_Engine->logicalDevice.allocateMemory (allocInfo);

  p_Engine->logicalDevice.bindImageMemory (image, memory, 0);

  // transition image to transfer dst
  transitionImageLayout (&image, vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eTransferDstOptimal);

  // copy staging buffer to image
  vk::CommandBuffer commandBuffer = beginCommandBuffer ();

  vk::BufferImageCopy region;
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = vk::Offset3D{ 0, 0, 0 };
  region.imageExtent = vk::Extent3D{ static_cast<uint32_t> (width),
                                     static_cast<uint32_t> (height), 1 };

  commandBuffer.copyBufferToImage (
      stagingBuffer, image, vk::ImageLayout::eTransferDstOptimal, region);

  endCommandBuffer (commandBuffer);

  // transition image to shader read only
  transitionImageLayout (&image, vk::ImageLayout::eTransferDstOptimal,
                         vk::ImageLayout::eShaderReadOnlyOptimal);

  // create view into image
  vk::ImageViewCreateInfo viewInfo;
  viewInfo.image = image;
  viewInfo.viewType = vk::ImageViewType::e2D;
  viewInfo.format = p_ImageCreateInfo.format;
  viewInfo.components = {
    vk::ComponentSwizzle::eR,
    vk::ComponentSwizzle::eG,
    vk::ComponentSwizzle::eB,
    vk::ComponentSwizzle::eA,
  };
  viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  // create image view
  imageView = p_Engine->logicalDevice.createImageView (viewInfo);

  // create image sampler
  vk::SamplerCreateInfo samplerInfo;
  samplerInfo.magFilter = vk::Filter::eLinear;
  samplerInfo.minFilter = vk::Filter::eLinear;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy
      = p_Engine->physicalProperties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = vk::CompareOp::eAlways;
  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

  // create sampler
  sampler = p_Engine->logicalDevice.createSampler (samplerInfo);

  // cleanup staging resources
  p_Engine->logicalDevice.destroyBuffer (stagingBuffer);
  p_Engine->logicalDevice.freeMemory (stagingMemory);

  // setup the descriptor
  descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  descriptor.imageView = imageView;
  descriptor.sampler = sampler;

  return;
}

void
Image::createStagingBuffer (vk::DeviceSize &p_BufferSize,
                            vk::Buffer &p_StagingBuffer,
                            vk::DeviceMemory &p_StagingMemory)
{
  vk::MemoryRequirements stagingReq;

  vk::BufferCreateInfo bufferInfo;
  bufferInfo.size = p_BufferSize;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

  // create staging buffer
  p_StagingBuffer = engine->logicalDevice.createBuffer (bufferInfo);

  // get memory requirements
  stagingReq
      = engine->logicalDevice.getBufferMemoryRequirements (p_StagingBuffer);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = stagingReq.size;
  allocInfo.memoryTypeIndex = engine->getMemoryType (
      stagingReq.memoryTypeBits,
      vk::MemoryPropertyFlagBits::eHostVisible
          | vk::MemoryPropertyFlagBits::eHostCoherent);

  // allocate staging memory
  p_StagingMemory = engine->logicalDevice.allocateMemory (allocInfo);

  // bind staging buffer & memory
  engine->logicalDevice.bindBufferMemory (p_StagingBuffer, p_StagingMemory, 0);
  return;
}

vk::CommandBuffer
Image::beginCommandBuffer (void)
{
  if (!engine->commandPool)
    throw std::runtime_error ("Command pool not initialized in mv device "
                              "handler :: image handler");

  vk::CommandBufferAllocateInfo allocInfo;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = engine->commandPool;
  allocInfo.commandBufferCount = 1;

  // create command buffer
  std::vector<vk::CommandBuffer> cmdBuffers
      = engine->logicalDevice.allocateCommandBuffers (allocInfo);

  if (cmdBuffers.size () < 1)
    throw std::runtime_error (
        "Failed to allocate command buffers :: image handler");

  // destroy other command buffers if more than 1 created
  // shouldn't happen
  if (cmdBuffers.size () > 1)
    {
      std::vector<vk::CommandBuffer> toDestroy;
      for (auto &buf : cmdBuffers)
        {
          if (&buf - &cmdBuffers[0] > 0)
            {
              toDestroy.push_back (buf);
            }
        }
      engine->logicalDevice.freeCommandBuffers (engine->commandPool,
                                                toDestroy);
    }

  vk::CommandBufferBeginInfo begin_info;
  begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  cmdBuffers.at (0).begin (begin_info);

  return cmdBuffers.at (0);
}

void
Image::endCommandBuffer (vk::CommandBuffer p_CommandBuffer)
{

  if (!engine->commandPool)
    throw std::runtime_error (
        "Command pool invalid ending commands :: image handler");

  if (!engine->graphicsQueue)
    throw std::runtime_error ("Graphics queue reference invalid ending "
                              "commands :: image handler");

  p_CommandBuffer.end ();

  vk::SubmitInfo submitInfo;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &p_CommandBuffer;

  engine->graphicsQueue.submit (submitInfo);
  engine->graphicsQueue.waitIdle ();

  engine->logicalDevice.freeCommandBuffers (engine->commandPool,
                                            p_CommandBuffer);
  return;
}

void
Image::transitionImageLayout (vk::Image *p_TargetImage,
                              vk::ImageLayout p_StartingLayout,
                              vk::ImageLayout p_EndingLayout)
{
  vk::CommandBuffer commandBuffer = beginCommandBuffer ();

  vk::ImageMemoryBarrier barrier;
  barrier.oldLayout = p_StartingLayout;
  barrier.newLayout = p_EndingLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = *p_TargetImage;
  barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vk::PipelineStageFlags sourceStage;
  vk::PipelineStageFlags destinationStage;

  if (p_StartingLayout == vk::ImageLayout::eUndefined
      && p_EndingLayout == vk::ImageLayout::eTransferDstOptimal)
    {
      barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
      barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

      sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
      destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
  else if (p_StartingLayout == vk::ImageLayout::eTransferDstOptimal
           && p_EndingLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
    {
      barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
      barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

      sourceStage = vk::PipelineStageFlagBits::eTransfer;
      destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
  else
    {
      throw std::runtime_error ("Layout transition requested is unsupported");
    }

  commandBuffer.pipelineBarrier (sourceStage, destinationStage,
                                 vk::DependencyFlags (), nullptr, nullptr,
                                 barrier);

  endCommandBuffer (commandBuffer);
  return;
}

void
Image::destroy (void)
{
  if (sampler)
    {
      engine->logicalDevice.destroySampler (sampler);
      sampler = nullptr;
    }
  if (imageView)
    {
      engine->logicalDevice.destroyImageView (imageView);
      imageView = nullptr;
    }
  if (image)
    {
      engine->logicalDevice.destroyImage (image);
      image = nullptr;
    }
  if (memory)
    {
      engine->logicalDevice.freeMemory (memory);
      memory = nullptr;
    }
  return;
}