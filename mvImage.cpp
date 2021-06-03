#define STB_IMAGE_IMPLEMENTATION
#include "mvImage.h"

#include "mvHelper.h"

Image::Image (void) { return; }

Image::~Image () { return; }

void
Image::create (const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice,
               const size_t p_ImageWidth,
               const size_t p_ImageHeight,
               const vk::Format p_DepthFormat,
               const vk::SampleCountFlagBits p_SampleCount)
{
  if (!p_PhysicalDevice)
    throw std::runtime_error ("[-] Invalid physical device handle passed to image object");
  if (!p_LogicalDevice)
    throw std::runtime_error ("[-] Invalid logical device handle passed to image object");

  physicalDevice = &p_PhysicalDevice;
  logicalDevice = &p_LogicalDevice;

  { // Create the image & allocate memory for it

    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = p_DepthFormat;
    imageInfo.extent = vk::Extent3D{ p_ImageWidth, p_ImageHeight, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = p_SampleCount;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    try
      {
        image = logicalDevice->createImage (imageInfo);
      }
    catch (vk::SystemError &e)
      {
        throw std::runtime_error ("Failed to create depth stencil image => "
                                  + std::string (e.what ()));
      }

    vk::MemoryRequirements memReq;
    memReq = logicalDevice->getImageMemoryRequirements (image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = Helper::getMemoryType (
        *physicalDevice, memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    try
      {
        memory = logicalDevice->allocateMemory (allocInfo);
      }
    catch (vk::SystemError &e)
      {
        throw std::runtime_error ("Failed to allocate depth stencil memory => "
                                  + std::string (e.what ()));
      }

    try
      {
        logicalDevice->bindImageMemory (image, memory, 0);
      }
    catch (vk::SystemError &e)
      {
        throw std::runtime_error ("Failed to bind depth stencil image/memory => "
                                  + std::string (e.what ()));
      }
  }

  { // Create a view into the depth stencil image

    vk::ImageViewCreateInfo viewInfo;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.image = image;
    viewInfo.format = p_DepthFormat;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    // if physical device supports high enough format add stenciling
    if (p_DepthFormat >= vk::Format::eD16UnormS8Uint)
      {
        viewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
      }

    try
      {
        imageView = logicalDevice->createImageView (viewInfo);
      }
    catch (vk::SystemError &e)
      {
        throw std::runtime_error ("Failed to create view for depth/stencil image => "
                                  + std::string (e.what ()));
      }
  }

  return;
}

void
Image::create (const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice,
               const size_t p_ImageWidth,
               const size_t p_ImageHeight,
               const ImageCreateInfo &p_ImageCreateInfo)
{
  if (!p_PhysicalDevice)
    throw std::runtime_error ("Invalid physical device handle passed to image");
  if (!p_LogicalDevice)
    throw std::runtime_error ("Invalid logical device handle passed to image");

  physicalDevice = &p_PhysicalDevice;
  logicalDevice = &p_LogicalDevice;

  vk::ImageCreateInfo imageInfo;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.format = p_ImageCreateInfo.format;
  imageInfo.extent.width = p_ImageWidth;
  imageInfo.extent.height = p_ImageHeight;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = p_ImageCreateInfo.samples;
  imageInfo.tiling = p_ImageCreateInfo.tiling;
  imageInfo.usage = p_ImageCreateInfo.usage; // VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                             // | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;

  image = p_LogicalDevice.createImage (imageInfo);

  memoryRequirements = p_LogicalDevice.getImageMemoryRequirements (image);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = memoryRequirements.size;
  allocInfo.memoryTypeIndex = getMemoryType (
      p_PhysicalDevice, memoryRequirements.memoryTypeBits, p_ImageCreateInfo.memoryProperties);

  // allocate image memory
  memory = p_LogicalDevice.allocateMemory (allocInfo);

  p_LogicalDevice.bindImageMemory (image, memory, 0);

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
  imageView = p_LogicalDevice.createImageView (viewInfo);
  return;
}

void
Image::create (const vk::PhysicalDevice &p_PhysicalDevice,
               const vk::Device &p_LogicalDevice,
               const vk::CommandPool &p_CommandPool,
               const vk::Queue &p_GraphicsQueue,
               ImageCreateInfo &p_ImageCreateInfo,
               std::string p_ImageFilename)
{
  // sanity check
  if (image || imageView || sampler || memory)
    throw std::runtime_error (
        "You already called create() for this object bozo; Call destroy() first!");
  if (!p_PhysicalDevice)
    throw std::runtime_error ("Invalid physical device handle passed to image");
  if (!p_LogicalDevice)
    throw std::runtime_error ("Invalid logical device handle passed to image");
  if (!p_CommandPool)
    throw std::runtime_error ("Invalid command pool handle passed to image");

  physicalDevice = &p_PhysicalDevice;
  logicalDevice = &p_LogicalDevice;

  int width = 0;
  int height = 0;
  int channels = 0;

  // load image
  stbi_uc *rawImage
      = stbi_load (p_ImageFilename.c_str (), &width, &height, &channels, STBI_rgb_alpha);
  vk::DeviceSize imageSize = width * height * 4;

  if (!rawImage)
    throw std::runtime_error ("Failed to load image => " + p_ImageFilename);

  vk::Buffer stagingBuffer;
  vk::DeviceMemory stagingMemory;
  void *stagingMapped = nullptr;

  // create staging buffer
  createStagingBuffer (p_PhysicalDevice, p_LogicalDevice, imageSize, stagingBuffer, stagingMemory);

  stagingMapped = p_LogicalDevice.mapMemory (stagingMemory, 0, imageSize);

  // copy raw image to staging memory
  memcpy (stagingMapped, rawImage, static_cast<size_t> (imageSize));

  p_LogicalDevice.unmapMemory (stagingMemory);

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
  imageInfo.samples = p_ImageCreateInfo.samples;
  imageInfo.tiling = p_ImageCreateInfo.tiling;
  imageInfo.usage = p_ImageCreateInfo.usage; // VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                             // | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;

  image = p_LogicalDevice.createImage (imageInfo);

  memoryRequirements = p_LogicalDevice.getImageMemoryRequirements (image);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = memoryRequirements.size;
  allocInfo.memoryTypeIndex = getMemoryType (
      p_PhysicalDevice, memoryRequirements.memoryTypeBits, p_ImageCreateInfo.memoryProperties);

  // allocate image memory
  memory = p_LogicalDevice.allocateMemory (allocInfo);

  p_LogicalDevice.bindImageMemory (image, memory, 0);

  // transition image to transfer dst
  transitionImageLayout (p_LogicalDevice,
                         p_CommandPool,
                         p_GraphicsQueue,
                         &image,
                         vk::ImageLayout::eUndefined,
                         vk::ImageLayout::eTransferDstOptimal);

  // copy staging buffer to image
  vk::CommandBuffer commandBuffer = beginCommandBuffer (p_LogicalDevice, p_CommandPool);

  vk::BufferImageCopy region;
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = vk::Offset3D{ 0, 0, 0 };
  region.imageExtent
      = vk::Extent3D{ static_cast<uint32_t> (width), static_cast<uint32_t> (height), 1 };

  commandBuffer.copyBufferToImage (
      stagingBuffer, image, vk::ImageLayout::eTransferDstOptimal, region);

  endCommandBuffer (p_LogicalDevice, p_CommandPool, p_GraphicsQueue, commandBuffer);

  // transition image to shader read only
  transitionImageLayout (p_LogicalDevice,
                         p_CommandPool,
                         p_GraphicsQueue,
                         &image,
                         vk::ImageLayout::eTransferDstOptimal,
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
  imageView = p_LogicalDevice.createImageView (viewInfo);

  // create image sampler
  auto physicalProperties = p_PhysicalDevice.getProperties ();
  vk::SamplerCreateInfo samplerInfo;
  samplerInfo.magFilter = vk::Filter::eLinear;
  samplerInfo.minFilter = vk::Filter::eLinear;
  samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
  samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
  samplerInfo.anisotropyEnable = VK_TRUE;
  samplerInfo.maxAnisotropy = physicalProperties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = vk::CompareOp::eAlways;
  samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

  // create sampler
  sampler = p_LogicalDevice.createSampler (samplerInfo);

  // cleanup staging resources
  p_LogicalDevice.destroyBuffer (stagingBuffer);
  p_LogicalDevice.freeMemory (stagingMemory);

  // setup the descriptor
  descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  descriptor.imageView = imageView;
  descriptor.sampler = sampler;

  return;
}

void
Image::createStagingBuffer (const vk::PhysicalDevice &p_PhysicalDevice,
                            const vk::Device &p_LogicalDevice,
                            vk::DeviceSize &p_BufferSize,
                            vk::Buffer &p_StagingBuffer,
                            vk::DeviceMemory &p_StagingMemory)
{
  vk::MemoryRequirements stagingReq;

  vk::BufferCreateInfo bufferInfo;
  bufferInfo.size = p_BufferSize;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

  // create staging buffer
  p_StagingBuffer = p_LogicalDevice.createBuffer (bufferInfo);

  // get memory requirements
  stagingReq = p_LogicalDevice.getBufferMemoryRequirements (p_StagingBuffer);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = stagingReq.size;
  allocInfo.memoryTypeIndex = getMemoryType (p_PhysicalDevice,
                                             stagingReq.memoryTypeBits,
                                             vk::MemoryPropertyFlagBits::eHostVisible
                                                 | vk::MemoryPropertyFlagBits::eHostCoherent);

  // allocate staging memory
  p_StagingMemory = p_LogicalDevice.allocateMemory (allocInfo);

  // bind staging buffer & memory
  p_LogicalDevice.bindBufferMemory (p_StagingBuffer, p_StagingMemory, 0);
  return;
}

vk::CommandBuffer
Image::beginCommandBuffer (const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool)
{
  if (!p_CommandPool)
    throw std::runtime_error ("Invalid command pool handle passed to image");

  vk::CommandBufferAllocateInfo allocInfo;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = p_CommandPool;
  allocInfo.commandBufferCount = 1;

  // create command buffer
  std::vector<vk::CommandBuffer> cmdBuffers = p_LogicalDevice.allocateCommandBuffers (allocInfo);

  if (cmdBuffers.size () < 1)
    throw std::runtime_error ("Failed to allocate command buffers :: image handler");

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
      p_LogicalDevice.freeCommandBuffers (p_CommandPool, toDestroy);
    }

  vk::CommandBufferBeginInfo begin_info;
  begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  cmdBuffers.at (0).begin (begin_info);

  return cmdBuffers.at (0);
}

void
Image::endCommandBuffer (const vk::Device &p_LogicalDevice,
                         const vk::CommandPool &p_CommandPool,
                         const vk::Queue &p_GraphicsQueue,
                         vk::CommandBuffer p_CommandBuffer)
{

  if (!p_CommandPool)
    throw std::runtime_error ("Invalid command pool handle passed to image");

  if (!p_GraphicsQueue)
    throw std::runtime_error ("Invalid graphics queue handle passed to image");

  p_CommandBuffer.end ();

  vk::SubmitInfo submitInfo;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &p_CommandBuffer;

  p_GraphicsQueue.submit (submitInfo);
  p_GraphicsQueue.waitIdle ();

  p_LogicalDevice.freeCommandBuffers (p_CommandPool, p_CommandBuffer);
  return;
}

void
Image::transitionImageLayout (const vk::Device &p_LogicalDevice,
                              const vk::CommandPool &p_CommandPool,
                              const vk::Queue &p_GraphicsQueue,
                              vk::Image *p_TargetImage,
                              vk::ImageLayout p_StartingLayout,
                              vk::ImageLayout p_EndingLayout)
{
  vk::CommandBuffer commandBuffer = beginCommandBuffer (p_LogicalDevice, p_CommandPool);

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

  commandBuffer.pipelineBarrier (
      sourceStage, destinationStage, vk::DependencyFlags (), nullptr, nullptr, barrier);

  endCommandBuffer (p_LogicalDevice, p_CommandPool, p_GraphicsQueue, commandBuffer);
  return;
}

void
Image::destroy (void)
{
  if (!logicalDevice) // nothing probably loaded but if there is..nothing we can do from here
    return;
  if (sampler)
    {
      logicalDevice->destroySampler (sampler);
      sampler = nullptr;
    }
  if (imageView)
    {
      logicalDevice->destroyImageView (imageView);
      imageView = nullptr;
    }
  if (image)
    {
      logicalDevice->destroyImage (image);
      image = nullptr;
    }
  if (memory)
    {
      logicalDevice->freeMemory (memory);
      memory = nullptr;
    }
  return;
}