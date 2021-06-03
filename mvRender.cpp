#include "mvRender.h"

#include "mvHelper.h"

Renderer::Renderer (vk::PhysicalDevice *p_PhysicalDevice,
                    vk::Device *p_LogicalDevice,
                    Swap *p_Swapchain,
                    QueueIndexInfo p_QueueIndices)
{
  if (!p_PhysicalDevice)
    throw std::runtime_error ("Invalid physical device handle passed to renderer");
  if (!p_LogicalDevice)
    throw std::runtime_error ("Invalid logical device handle passed to renderer");
  if (!p_Swapchain)
    throw std::runtime_error ("Invalid swapchain handle passed to renderer");

  physicalDevice = p_PhysicalDevice;
  logicalDevice = p_LogicalDevice;
  swapchain = p_Swapchain;
  queueIdx = p_QueueIndices;

  pipelines = std::make_unique<PipelineMap> ();
  pipelineLayouts = std::make_unique<PipelineLayoutMap> ();
  depthStencil = std::make_unique<Image> ();
  renderPasses = std::make_unique<RenderPassMap> ();
  framebuffers = std::make_unique<FramebufferMap> ();
  commandPools = std::make_unique<CommandPoolMap> ();
  commandQueues = std::make_unique<CommandQueueMap> ();
  inFlightFences = std::make_unique<std::vector<vk::Fence>> ();
  waitFences = std::make_unique<std::vector<vk::Fence>> ();
  semaphores = std::make_unique<SemaphoreMap> ();
  return;
}

Renderer::~Renderer () {}

void
Renderer::reset (void)
{
  destroyCommandPools ();

  destroyFramebuffers ();

  destroyDepthStencil ();

  destroyPipelines ();

  destroyRenderPasses ();
  return;
}

void
Renderer::cleanup (void)
{
  destroyPipelines ();

  destroyPipelineLayouts ();

  destroyCommandPools ();

  destroyFences ();

  destroySemaphores ();

  destroyDepthStencil ();

  destroyRenderPasses ();

  destroyFramebuffers ();
  return;
}

void
Renderer::initialize (void)
{
  {
    using enum vk::QueueFlagBits;

    vk::QueueFlags flags;

    if (queueIdx.graphics != UINT32_MAX)
      flags |= eGraphics;
    if (queueIdx.compute != UINT32_MAX)
      flags |= eCompute;
    if (queueIdx.transfer != UINT32_MAX)
      throw std::runtime_error ("No transfer queue support at the moment");
    //   flags |= eTransfer;

    if (!static_cast<uint32_t> (flags))
      throw std::runtime_error ("No queue indices specified; Could not set flags");

    createCommandPools (flags);
    getCommandQueues (flags);
  } // get pools/queues
  createCommandBuffers ();
  createSynchronizationPrimitives ();
  setupDepthStencil ();
  setupRenderPass ();
  setupFramebuffers ();
}

void
Renderer::createSynchronizationPrimitives (void)
{
  vk::FenceCreateInfo fenceInfo;
  fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

  { // create fences
    if (!waitFences)
      {
        std::cout << "[-] Requested creation of waitFences but it was never initialized\n";
        waitFences = std::make_unique<std::vector<vk::Fence>> ();
      }
    if (!inFlightFences)
      {
        std::cout << "[-] Requested creation of inFlightFences but it was never initialized\n";
        inFlightFences = std::make_unique<std::vector<vk::Fence>> ();
      }
    waitFences->resize (swapchain->buffers.size (), nullptr);
    inFlightFences->resize (MAX_IN_FLIGHT);
  }

  for (auto &fence : *inFlightFences)
    {
      fence = logicalDevice->createFence (fenceInfo);
    }

  { // create semaphores
    using enum mv::SemaphoreType;
    if (!semaphores)
      {
        std::cout << "[-] Requested creation of semaphores but it was never initialized\n";
        semaphores = std::make_unique<SemaphoreMap> ();
      }
    if (!semaphores->contains (ePresentComplete))
      semaphores->insert ({ ePresentComplete, {} });
    if (!semaphores->contains (eRenderComplete))
      semaphores->insert ({ eRenderComplete, {} });
    if (!semaphores->contains (eComputeComplete))
      semaphores->insert ({ eComputeComplete, {} });
    vk::SemaphoreCreateInfo semaphoreInfo;
    semaphores->at (ePresentComplete) = logicalDevice->createSemaphore (semaphoreInfo);
    semaphores->at (eRenderComplete) = logicalDevice->createSemaphore (semaphoreInfo);
    semaphores->at (eComputeComplete) = logicalDevice->createSemaphore (semaphoreInfo);
  }
  return;
}

void
Renderer::setupDepthStencil (void)
{
  if (!depthStencil)
    {
      std::cout << "[-] Requested creation of depth stencil but it was never initialized\n";
      depthStencil = std::make_unique<Image> ();
    }

  depthStencil->create (*physicalDevice,
                        *logicalDevice,
                        swapchain->swapExtent.width,
                        swapchain->swapExtent.height,
                        swapchain->depthFormat,
                        swapchain->sampleCount);
  return;
}

void
Renderer::createCommandBuffers (void)
{
  if (!commandPools)
    {
      std::cout << "[-] Requested creation of command buffers but command pools handle does not "
                   "exists!\n";
      commandPools = std::make_unique<CommandPoolMap> ();
    }
  if (!swapchain)
    {
      throw std::runtime_error (
          "Requested to create command buffers but swapchain never initialized");
    }

  for (auto &[flag, pair] : *commandPools)
    {
      auto &[pool, buffer] = pair;
      if (!pool)
        {
          throw std::runtime_error ("Requested creation of command buffers but pool type ["
                                    + std::to_string (static_cast<uint32_t> (flag))
                                    + "] was never created");
        }
      if (flag == vk::QueueFlagBits::eGraphics && queueIdx.graphics != UINT32_MAX)
        {
          buffer.resize (swapchain->buffers.size ());

          vk::CommandBufferAllocateInfo allocInfo;
          allocInfo.level = vk::CommandBufferLevel::ePrimary;
          allocInfo.commandPool = pool;
          allocInfo.commandBufferCount = static_cast<uint32_t> (buffer.size ());

          buffer = logicalDevice->allocateCommandBuffers (allocInfo);

          if (buffer.empty ())
            throw std::runtime_error ("Failed to allocate command buffers");
        }
      if (flag == vk::QueueFlagBits::eCompute && queueIdx.compute != UINT32_MAX)
        {
          buffer.resize (swapchain->buffers.size ());

          vk::CommandBufferAllocateInfo allocInfo;
          allocInfo.level = vk::CommandBufferLevel::ePrimary;
          allocInfo.commandPool = pool;
          allocInfo.commandBufferCount = static_cast<uint32_t> (buffer.size ());

          buffer = logicalDevice->allocateCommandBuffers (allocInfo);

          if (buffer.empty ())
            throw std::runtime_error ("Failed to allocate command buffers");
        }
    }
  return;
}

void
Renderer::setupRenderPass (void)
{
  std::array<vk::AttachmentDescription, 3> coreAttachments;
  // MSAA resolve target -> ImGui
  coreAttachments[0].format = swapchain->colorFormat;
  coreAttachments[0].samples = swapchain->sampleCount;
  coreAttachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  coreAttachments[0].storeOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  coreAttachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[0].initialLayout = vk::ImageLayout::eUndefined;
  coreAttachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal; // passed to imgui

  // Depth attachment
  coreAttachments[1].format = swapchain->depthFormat;
  coreAttachments[1].samples = swapchain->sampleCount;
  coreAttachments[1].loadOp = vk::AttachmentLoadOp::eClear;
  coreAttachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  coreAttachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[1].initialLayout = vk::ImageLayout::eUndefined;
  coreAttachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  // Color attachment
  coreAttachments[2].format = swapchain->colorFormat;
  coreAttachments[2].samples = vk::SampleCountFlagBits::e1;
  coreAttachments[2].loadOp = vk::AttachmentLoadOp::eClear;
  coreAttachments[2].storeOp = vk::AttachmentStoreOp::eStore;
  coreAttachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  coreAttachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[2].initialLayout = vk::ImageLayout::eUndefined;
  coreAttachments[2].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  /*
      Core render references
  */
  vk::AttachmentReference colorRef;
  colorRef.attachment = 0;
  colorRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentReference depthRef;
  depthRef.attachment = 1;
  depthRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  vk::AttachmentReference msaaRef;
  msaaRef.attachment = 2;
  msaaRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  // color attachment and depth testing subpass
  vk::SubpassDescription corePass;
  corePass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  corePass.colorAttachmentCount = 1;
  corePass.pColorAttachments = &colorRef;
  corePass.pDepthStencilAttachment = &depthRef;
  corePass.pResolveAttachments = &msaaRef;

  std::vector<vk::SubpassDescription> coreSubpasses = { corePass };

  std::array<vk::SubpassDependency, 1> coreDependencies;
  // Color + depth subpass
  coreDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  coreDependencies[0].dstSubpass = 0;
  coreDependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
  coreDependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  coreDependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
  coreDependencies[0].dstAccessMask
      = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
  coreDependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  vk::RenderPassCreateInfo coreRenderPassInfo;
  coreRenderPassInfo.attachmentCount = static_cast<uint32_t> (coreAttachments.size ());
  coreRenderPassInfo.pAttachments = coreAttachments.data ();
  coreRenderPassInfo.subpassCount = static_cast<uint32_t> (coreSubpasses.size ());
  coreRenderPassInfo.pSubpasses = coreSubpasses.data ();
  coreRenderPassInfo.dependencyCount = static_cast<uint32_t> (coreDependencies.size ());
  coreRenderPassInfo.pDependencies = coreDependencies.data ();

  using enum mv::RenderPassType;
  try
    {
      renderPasses->insert ({
          eCore,
          logicalDevice->createRenderPass (coreRenderPassInfo),
      });
    }
  catch (vk::SystemError &e)
    {
      throw std::runtime_error ("Vulkan error creating core render pass => "
                                + std::string (e.what ()));
    }
  catch (std::exception &e)
    {
      throw std::runtime_error ("Std error creating core render pass => "
                                + std::string (e.what ()));
    }
  return;
}

void
Renderer::setupFramebuffers (void)
{
  std::cout << "[+] Creating core framebuffers\n";
  // Attachments for core engine rendering
  std::array<vk::ImageView, 3> coreAttachments;

  if (!depthStencil)
    throw std::runtime_error ("Requested framebuffer creation but depth/stencil never initialized");

  if (!depthStencil->imageView)
    throw std::runtime_error ("Requested framebuffer creation but depth/stencil view is nullptr");

  if (!framebuffers)
    {
      std::cout
          << "[-] Requested creation of framebuffers but framebuffer object never initialized\n";
      framebuffers = std::make_unique<FramebufferMap> ();
    }

  // each frame buffer uses same depth image
  coreAttachments[1] = depthStencil->imageView;

  // core engine render will use color attachment buffer & depth buffer
  using enum mv::RenderPassType;
  using enum mv::FramebufferType;
  vk::FramebufferCreateInfo coreFrameInfo;
  coreFrameInfo.renderPass = renderPasses->at (eCore);
  coreFrameInfo.attachmentCount = static_cast<uint32_t> (coreAttachments.size ());
  coreFrameInfo.pAttachments = coreAttachments.data ();
  coreFrameInfo.width = swapchain->swapExtent.width;
  coreFrameInfo.height = swapchain->swapExtent.height;
  coreFrameInfo.layers = 1;

  // Framebuffer per swap image
  if (!framebuffers->contains (eCoreFramebuffer))
    {
      framebuffers->insert ({
          { eCoreFramebuffer, { {} } },
      });
    }
  framebuffers->at (eCoreFramebuffer).resize (static_cast<uint32_t> (swapchain->buffers.size ()));
  for (size_t i = 0; i < framebuffers->at (eCoreFramebuffer).size (); i++)
    {
      if (!swapchain->buffers.at (i).image)
        throw std::runtime_error ("Swapchain buffer at index " + std::to_string (i)
                                  + " has nullptr image");
      if (!swapchain->buffers.at (i).view)
        throw std::runtime_error ("Swapchain buffer at index " + std::to_string (i)
                                  + " has nullptr view");
      // Assign msaa buffer -- main target
      coreAttachments[0] = swapchain->buffers.at (i).msaaImage->imageView;
      // Assign each swapchain image to a frame buffer
      coreAttachments[2] = swapchain->buffers.at (i).view;

      framebuffers->at (eCoreFramebuffer).at (i) = logicalDevice->createFramebuffer (coreFrameInfo);
      std::cout << "Created core frame buffer => " << framebuffers->at (eCoreFramebuffer).at (i)
                << "\n";
    }
  return;
}

inline void
Renderer::createCommandPools (vk::QueueFlags p_QueueFlags)
{
  if (!commandPools)
    commandPools = std::make_unique<CommandPoolMap> ();

  { // ensure requested pools exist
    using enum vk::QueueFlagBits;
    if (!commandPools->contains (eGraphics) && (p_QueueFlags & eGraphics))
      {
        commandPools->insert ({
            { eGraphics, { Helper::createCommandPool (eGraphics), { {} } } },
        });
      }
    else
      {
        std::cout << "[-] Requested graphics command pool but it already exists\n";
      }
    if (!commandPools->contains (eCompute) && (p_QueueFlags & eCompute))
      {
        commandPools->insert ({ {
            eGraphics,
            { Helper::createCommandPool (eGraphics), { {} } },
        } });
      }
    else
      {
        std::cout << "[-] Requested compute queue but it already exsits\n";
      }
  }
  return;
}

inline void
Renderer::getCommandQueues (vk::QueueFlags p_QueueFlags)
{
  using enum vk::QueueFlagBits;
  if (!commandQueues)
    commandQueues = std::make_unique<CommandQueueMap> ();

  if (commandQueues->contains (eGraphics) && (p_QueueFlags & eGraphics))
    {
      if (!commandQueues->at (eGraphics))
        {
          commandQueues->insert ({
              { eGraphics, { logicalDevice->getQueue (queueIdx.graphics, 0) } },
          });
        }
    }
  else
    {
      std::cout << "[-] Requested graphics queue but command pool not created\n";
    }
  if (commandQueues->contains (eCompute) && (p_QueueFlags & eCompute))
    {
      if (!commandQueues->at (eCompute))
        {
          commandQueues->insert ({
              { eGraphics, { logicalDevice->getQueue (queueIdx.compute, 0) } },
          });
        }
    }
  else
    {
      std::cout << "[-] Requested compute queue but command pool not created\n";
    }
  return;
}

/*
    Cleanup methods
*/
inline void
Renderer::destroyCommandPools (void)
{
  // destroy command pool/buffers
  if (commandPools && !commandPools->empty ())
    {
      for (auto &[queueFlag, pair] : *commandPools)
        {
          auto &[pool, buffer] = pair;
          logicalDevice->freeCommandBuffers (pool, buffer);
          logicalDevice->destroyCommandPool (pool);
        }
      commandPools.reset ();
    }
  return;
}

inline void
Renderer::destroyFramebuffers (void)
{
  // cleanup framebuffers
  if (framebuffers && !framebuffers->empty ())
    {
      for (auto &[type, framebuffers] : *framebuffers)
        {
          if (!framebuffers.empty ())
            {
              for (auto &buffer : framebuffers)
                {
                  logicalDevice->destroyFramebuffer (buffer);
                }
            }
        }
      framebuffers.reset ();
    }
  return;
}

inline void
Renderer::destroyDepthStencil (void)
{
  if (depthStencil)
    {
      depthStencil->destroy ();
      depthStencil.reset ();
    }
  return;
}

inline void
Renderer::destroyPipelines (void)
{
  if (pipelines && !pipelines->empty ())
    {
      for (auto &pipeline : *pipelines)
        {
          if (pipeline.second)
            logicalDevice->destroyPipeline (pipeline.second);
        }
      pipelines.reset ();
    }
  return;
}

inline void
Renderer::destroyPipelineLayouts (void)
{
  if (pipelineLayouts && !pipelineLayouts->empty ())
    {
      for (auto &layout : *pipelineLayouts)
        {
          if (layout.second)
            logicalDevice->destroyPipelineLayout (layout.second);
        }
      pipelineLayouts.reset ();
    }
  return;
}

inline void
Renderer::destroyRenderPasses (void)
{
  // destroy render pass
  if (renderPasses && !renderPasses->empty ())
    {
      for (auto &[type, renderpass] : *renderPasses)
        {
          if (renderpass)
            logicalDevice->destroyRenderPass (renderpass);
        }
      renderPasses.reset ();
    }
  return;
}

inline void
Renderer::destroyFences (void)
{
  if (inFlightFences && !inFlightFences->empty ())
    {
      for (auto &fence : *inFlightFences)
        {
          if (fence)
            {
              logicalDevice->destroyFence (fence, nullptr);
            }
        }
      inFlightFences.reset ();
    }
  return;
}

inline void
Renderer::destroySemaphores (void)
{
  if (semaphores && !semaphores->empty ())
    {
      for (auto &[type, semaphore] : *semaphores)
        {
          logicalDevice->destroySemaphore (semaphore);
        }
      semaphores.reset ();
    }
  return;
}