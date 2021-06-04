#include "mvHelper.h"

void
LogHandler::logMessage (std::pair<LogHandlerMessagePriority, std::string> p_Message)
{
  trim ();
  messages.push_back (p_Message);
}

void
LogHandler::logMessage (LogHandlerMessagePriority p_MessagePriority, std::string p_Message)
{
  trim ();
  messages.push_back ({ p_MessagePriority, p_Message });
}

void
LogHandler::logMessage (std::string p_Message)
{
  trim ();
  messages.push_back ({ LogHandlerMessagePriority::eInfo, p_Message });
}

std::vector<std::pair<LogHandlerMessagePriority, std::string>>
LogHandler::getMessages (void)
{
  return messages; // return copy of list
}

// Create quick one time submit command buffer
vk::CommandBuffer
Helper::beginCommandBuffer (const vk::Device &p_LogicalDevice, const vk::CommandPool &p_CommandPool)
{
  if (!p_LogicalDevice)
    throw std::runtime_error ("Attempted to create one time submit command buffer but logical "
                              "device is nullptr :: Helper tools");
  if (!p_CommandPool)
    throw std::runtime_error ("attempted to create one time submit command buffer but command "
                              "pool is not initialized :: Helper tools\n");

  vk::CommandBufferAllocateInfo allocInfo;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandPool = p_CommandPool;
  allocInfo.commandBufferCount = 1;

  // allocate command buffer
  std::vector<vk::CommandBuffer> cmdbuf = p_logicalDevice->allocateCommandBuffers (allocInfo);

  if (cmdbuf.size () < 1)
    throw std::runtime_error ("Failed to create one time submit command buffer");

  // if more than 1 was created clean them up
  // shouldn't happen
  if (cmdbuf.size () > 1)
    {
      std::vector<vk::CommandBuffer> toDestroy;
      for (auto &buf : cmdbuf)
        {
          if (&buf - &cmdbuf[0] > 0)
            {
              toDestroy.push_back (buf);
            }
        }
      p_logicalDevice->freeCommandBuffers (p_CommandPool, toDestroy);
    }

  vk::CommandBufferBeginInfo beginInfo;
  beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

  cmdbuf->at (0).begin (beginInfo);

  return cmdbuf->at (0);
}

void
Helper::endCommandBuffer (const vk::Device &p_LogicalDevice,
                          const vk::CommandPool &p_CommandPool,
                          const vk::CommandBuffer &p_CommandBuffer,
                          const vk::Queue &p_GraphicsQueue)
{
  // sanity check
  if (!p_CommandPool)
    throw std::runtime_error ("attempted to end command buffer recording "
                              "but command pool is nullptr :: Helper tools");

  if (!p_CommandBuffer)
    throw std::runtime_error ("attempted to end command buffer recording but the buffer passed "
                              "as parameter is nullptr :: Helper tools");

  if (!p_GraphicsQueue)
    throw std::runtime_error ("attempted to end command buffer recording but graphics queue is "
                              "nullptr :: Helper tools");

  p_CommandBuffer.end ();

  vk::SubmitInfo submitInfo;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &p_CommandBuffer;

  // submit buffer
  p_GraphicsQueue.submit (submitInfo);
  p_GraphicsQueue.waitIdle ();

  // free buffer
  p_logicalDevice->freeCommandBuffers (p_CommandPool, p_CommandBuffer);
  return;
}

std::vector<std::string>
Helper::tokenize (const std::string p_ToSplit, const std::regex p_Regex)
{
  std::sregex_token_iterator it{ p_ToSplit.begin (), p_ToSplit.end (), p_Regex, -1 };
  std::vector<std::string> tokenized{ it, {} };

  // Trim '{' and '}'
  std::for_each (tokenized.begin (), tokenized.end (), [] (std::string &token) {
    // {
    auto pos = token.find ('{');
    while (pos != std::string::npos)
      {
        token.erase (pos, 1);
        pos = token.find ('{');
      }

    // }
    pos = token.find ('}');
    while (pos != std::string::npos)
      {
        token.erase (pos, 1);
        pos = token.find ('}');
      }
  });

  // Remove empty strings
  tokenized.erase (std::remove_if (tokenized.begin (),
                                   tokenized.end (),
                                   [] (const std::string &s) { return s.size () == 0; }),
                   tokenized.end ());

  return tokenized;
}

std::vector<char>
Helper::readFile (std::string p_Filename)
{
  size_t fileSize;
  std::ifstream file;
  std::vector<char> buffer;

  // check if file exists
  try
    {
      if (!std::filesystem::exists (p_Filename))
        throw std::runtime_error ("Shader file " + p_Filename + " does not exist");

      file.open (p_Filename, std::ios::ate | std::ios::binary);

      if (!file.is_open ())
        {
          std::ostringstream oss;
          oss << "Failed to open file " << p_Filename;
          throw std::runtime_error (oss.str ());
        }

      // prepare buffer to hold shader bytecode
      fileSize = (size_t)file.tellg ();
      buffer.resize (fileSize);

      // go back to beginning of file and read in
      file.seekg (0);
      file.read (buffer.data (), fileSize);
      file.close ();
    }
  catch (std::filesystem::filesystem_error &e)
    {
      std::ostringstream oss;
      oss << "Filesystem Exception : " << e.what () << std::endl;
      throw std::runtime_error (oss.str ());
    }
  catch (std::exception &e)
    {
      std::ostringstream oss;
      oss << "Standard Exception : " << e.what ();
      throw std::runtime_error (oss.str ());
    }
  catch (...)
    {
      throw std::runtime_error ("Unhandled exception while loading a file");
    }

  if (buffer.empty ())
    {
      throw std::runtime_error ("File reading operation returned empty buffer :: shaders?");
    }

  return buffer;
}

vk::ShaderModule
Helper::createShaderModule (const std::vector<char> &p_ShaderCharBuffer)
{
  vk::ShaderModuleCreateInfo moduleInfo;
  moduleInfo.codeSize = p_ShaderCharBuffer.size ();
  moduleInfo.pCode = reinterpret_cast<const uint32_t *> (p_ShaderCharBuffer.data ());

  vk::ShaderModule module = logicalDevice->createShaderModule (moduleInfo);

  return module;
}

vk::CommandPool
Helper::createCommandPool (const vk::Device &p_LogicalDevice,
                           uint32_t p_QueueIndex,
                           vk::CommandPoolCreateFlags p_CreateFlags)
{
  vk::CommandPoolCreateInfo poolInfo;
  poolInfo.flags = p_CreateFlags;
  poolInfo.queueFamilyIndex = p_QueueIndex;

  return p_logicalDevice->createCommandPool (poolInfo);
}

vk::CommandPool
Helper::createCommandPool (const vk::PhysicalDevice &p_PhysicalDevice,
                           const vk::Device &p_LogicalDevice,
                           vk::QueueFlagBits p_QueueFlag,
                           vk::CommandPoolCreateFlags p_CreateFlags)
{
  // fetch queue
}

vk::Format
Helper::getSupportedDepthFormat (void) const
{
  std::vector<vk::Format> depthFormats = {
    vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat, vk::Format::eD24UnormS8Uint,
    vk::Format::eD16UnormS8Uint,  vk::Format::eD16Unorm,
  };

  for (auto &format : depthFormats)
    {
      vk::FormatProperties formatProperties = physicalDevice.getFormatProperties (format);
      if (formatProperties.optimalTilingFeatures
          & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
          if (!(formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage))
            continue;
        }
      return format;
    }
  throw std::runtime_error ("Failed to find good format");
}

uint32_t
Helper::getMemoryType (const vk::PhysicalDevice &p_PhysicalDevice,
                       uint32_t p_MemoryTypeBits,
                       const vk::MemoryPropertyFlags p_MemoryProperties,
                       vk::Bool32 *p_IsMemoryTypeFound)
{
  auto physicalMemoryProperties = p_PhysicalDevice.getMemoryProperties ();
  for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
    {
      if ((p_MemoryTypeBits & 1) == 1)
        {
          if ((physicalMemoryProperties.memoryTypes[i].propertyFlags & p_MemoryProperties))
            {
              if (p_IsMemoryTypeFound)
                {
                  *p_IsMemoryTypeFound = true;
                }
              return i;
            }
        }
      p_MemoryTypeBits >>= 1;
    }

  if (p_IsMemoryTypeFound)
    {
      *p_IsMemoryTypeFound = false;
      return 0;
    }
  else
    {
      throw std::runtime_error ("Could not find a matching memory type");
    }
}

uint32_t
Helper::getQueueFamilyIndex (vk::QueueFlagBits p_QueueFlagBits)
{
  for (const auto &queueProperty : queueFamilyProperties)
    {
      if ((queueProperty.queueFlags & p_QueueFlagBits))
        {
          return (&queueProperty - &queueFamilyProperties[0]);
        }
    }

  throw std::runtime_error ("Could not find requested queue family :: "
                            + std::to_string (static_cast<uint32_t> (p_QueueFlagBits)));
}

// Create buffer with Vulkan buffer/memory objects
void
Helper::createBuffer (vk::BufferUsageFlags p_BufferUsageFlags,
                      vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                      vk::DeviceSize p_DeviceSize,
                      vk::Buffer *p_VkBuffer,
                      vk::DeviceMemory *p_DeviceMemory,
                      void *p_InitialData) const
{
  logger.logMessage ("Allocating buffer of size => "
                     + std::to_string (static_cast<uint32_t> (p_DeviceSize)));

  vk::BufferCreateInfo bufferInfo;
  bufferInfo.usage = p_BufferUsageFlags;
  bufferInfo.size = p_DeviceSize;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;

  // create vulkan buffer
  *p_VkBuffer = logicalDevice->createBuffer (bufferInfo);

  // allocate memory for buffer
  vk::MemoryRequirements memRequirements;
  vk::MemoryAllocateInfo allocInfo;

  memRequirements = logicalDevice->getBufferMemoryRequirements (*p_VkBuffer);

  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = getMemoryType (memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

  // Allocate memory
  *p_DeviceMemory = logicalDevice->allocateMemory (allocInfo);

  // Bind newly allocated memory to buffer
  logicalDevice->bindBufferMemory (*p_VkBuffer, *p_DeviceMemory, 0);

  // If data was passed to creation, load it
  if (p_InitialData != nullptr)
    {
      void *mapped = logicalDevice->mapMemory (*p_DeviceMemory, 0, p_DeviceSize);

      memcpy (mapped, p_InitialData, p_DeviceSize);

      logicalDevice->unmapMemory (*p_DeviceMemory);
    }

  return;
}

// create buffer with custom Buffer interface
void
Helper::createBuffer (vk::BufferUsageFlags p_BufferUsageFlags,
                      vk::MemoryPropertyFlags p_MemoryPropertyFlags,
                      MvBuffer *p_MvBuffer,
                      vk::DeviceSize p_DeviceSize,
                      void *p_InitialData) const
{
  // sanity check
  if (static_cast<uint32_t> (p_DeviceSize) == 0)
    throw std::runtime_error ("Invalid device size");
  logger.logMessage ("Allocating buffer of size => "
                     + std::to_string (static_cast<uint32_t> (p_DeviceSize)));

  // create buffer
  vk::BufferCreateInfo bufferInfo;
  bufferInfo.usage = p_BufferUsageFlags;
  bufferInfo.size = p_DeviceSize;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;

  p_MvBuffer->buffer = logicalDevice->createBuffer (bufferInfo);

  vk::MemoryRequirements memRequirements;
  vk::MemoryAllocateInfo allocInfo;

  memRequirements = logicalDevice->getBufferMemoryRequirements (p_MvBuffer->buffer);

  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = getMemoryType (memRequirements.memoryTypeBits, p_MemoryPropertyFlags);

  // allocate memory
  p_MvBuffer->memory = logicalDevice->allocateMemory (allocInfo);

  p_MvBuffer->alignment = memRequirements.alignment;
  p_MvBuffer->size = p_DeviceSize;
  p_MvBuffer->usageFlags = p_BufferUsageFlags;
  p_MvBuffer->memoryPropertyFlags = p_MemoryPropertyFlags;

  // bind buffer & memory
  p_MvBuffer->bind (logicalDevice);

  p_MvBuffer->setupBufferInfo ();

  if (p_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent
      && p_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)
    p_MvBuffer->canMap = true;

  // copy if necessary
  if (p_InitialData != nullptr)
    {
      if (p_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent
          && p_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        {

          p_MvBuffer->map (logicalDevice);

          memcpy (p_MvBuffer->mapped, p_InitialData, p_DeviceSize);

          p_MvBuffer->unmap (logicalDevice);
        }
      else
        {
          if (p_MemoryPropertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
            {
              vk::Buffer stagingBuffer;
              vk::DeviceMemory stagingMemory;
              void *stagingMapped = nullptr;

              // create staging buffer
              createStagingBuffer (p_DeviceSize, stagingBuffer, stagingMemory);

              // fill stage
              stagingMapped = logicalDevice->mapMemory (stagingMemory, 0, p_DeviceSize);
              memcpy (stagingMapped, p_InitialData, static_cast<size_t> (p_DeviceSize));
              logicalDevice->unmapMemory (stagingMemory);

              // copyto device local
              auto pool = commandPoolsBuffers->at (vk::QueueFlagBits::eGraphics)->at (0).first;
              auto cmdbuf = Helper::beginCommandBuffer (logicalDevice, pool);

              {
                vk::BufferCopy region;
                region.srcOffset = 0;
                region.dstOffset = 0;
                region.size = p_DeviceSize;
                cmdbuf.copyBuffer (stagingBuffer, p_MvBuffer->buffer, region);
              }

              // end, submit
              endCommandBuffer (pool, cmdbuf);

              logicalDevice->destroyBuffer (stagingBuffer);
              logicalDevice->freeMemory (stagingMemory);
            }
        }
    }

  return;
}

void
Helper::createStagingBuffer (vk::DeviceSize &p_BufferSize,
                             vk::Buffer &p_StagingBuffer,
                             vk::DeviceMemory &p_StagingMemory) const
{
  vk::MemoryRequirements stagingReq;

  vk::BufferCreateInfo bufferInfo;
  bufferInfo.size = p_BufferSize;
  bufferInfo.sharingMode = vk::SharingMode::eExclusive;
  bufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;

  // create staging buffer
  p_StagingBuffer = logicalDevice->createBuffer (bufferInfo);

  // get memory requirements
  stagingReq = logicalDevice->getBufferMemoryRequirements (p_StagingBuffer);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = stagingReq.size;
  allocInfo.memoryTypeIndex = getMemoryType (stagingReq.memoryTypeBits,
                                             vk::MemoryPropertyFlagBits::eHostVisible
                                                 | vk::MemoryPropertyFlagBits::eHostCoherent);

  // allocate staging memory
  p_StagingMemory = logicalDevice->allocateMemory (allocInfo);

  // bind staging buffer & memory
  logicalDevice->bindBufferMemory (p_StagingBuffer, p_StagingMemory, 0);
  return;
}