#include "mvRender.h"

#include "mvHelper.h"

#include "mvModel.h"

#include "mvState.h"

#include "mvGui.h"

Renderer::Renderer (vk::PhysicalDevice *p_PhysicalDevice,
                    vk::Device *p_LogicalDevice,
                    Swap *p_Swapchain,
                    GuiHandler *p_GuiHandler,
                    StateMachine *p_StateMachine,
                    QueueIndexInfo p_QueueIndices)
{
  if (!p_PhysicalDevice)
    throw std::runtime_error ("Invalid physical device handle passed to renderer");
  if (!p_LogicalDevice)
    throw std::runtime_error ("Invalid logical device handle passed to renderer");
  if (!p_Swapchain)
    throw std::runtime_error ("Invalid swapchain handle passed to renderer");
  if (!p_StateMachine)
    throw std::runtime_error ("Invalid state machine handle passed to renderer");
  if (!p_GuiHandler)
    throw std::runtime_error ("Invalid gui handle passed to renderer");

  physicalDevice = p_PhysicalDevice;
  logicalDevice = p_LogicalDevice;
  swapchain = p_Swapchain;
  gui = p_GuiHandler;
  stateMachine = p_StateMachine;
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

void
Renderer::prepareLayouts (void)
{
  using enum mv::PipelineType;
  vk::DescriptorSetLayout uniformLayout = getLayout (vk::DescriptorType::eUniformBuffer);
  vk::DescriptorSetLayout samplerLayout = getLayout (vk::DescriptorType::eCombinedImageSampler);

  { // m/v/p, texture, normal
    std::vector<vk::DescriptorSetLayout> layoutWSampler = {
      uniformLayout, // Object uniform
      uniformLayout, // View uniform
      uniformLayout, // Projection uniform
      samplerLayout, // Object texture sampler
      samplerLayout, // Object Normal sampler
    };
    createPipelineLayout (*pipelineLayouts, eMVPWSampler, layoutWSampler);
  }

  { // m/v/p
    std::vector<vk::DescriptorSetLayout> layoutNoSampler = {
      uniformLayout, // Object uniform
      uniformLayout, // View uniform
      uniformLayout, // Projection uniform
    };
    createPipelineLayout (*pipelineLayouts, eMVPNoSampler, layoutNoSampler);
  }

  { // v/p texture, normal
    std::vector<vk::DescriptorSetLayout> layoutTerrainMeshWSampler = {
      uniformLayout, // View uniform
      uniformLayout, // Projection uniform
      samplerLayout, // Terrain texture sampler
      samplerLayout, // Terrain normal sampler
    };
    createPipelineLayout (*pipelineLayouts, eVPWSampler, layoutTerrainMeshWSampler);
  }

  { // v/p
    std::vector<vk::DescriptorSetLayout> layoutTerrainMeshNoSampler = {
      uniformLayout, // View uniform
      uniformLayout, // Projection uniform
    };
    createPipelineLayout (*pipelineLayouts, eVPNoSampler, layoutTerrainMeshNoSampler);
  }

  return;
}

// creation of all compute related pipelines
void
Renderer::prepareComputePipelines (void)
{

  {
    using enum mv::PipelineType;
    auto storageBuffer = getLayout (vk::DescriptorType::eStorageBuffer);
    auto uniformBuffer = getLayout (vk::DescriptorType::eUniformBuffer);
    std::vector<vk::DescriptorSetLayout> computeDescriptors = {
      uniformBuffer, // clip space matrix
      storageBuffer, // vertex buffer
      storageBuffer, // index buffer
    };
    createPipelineLayout (*pipelineLayouts, eTerrainCompute, computeDescriptors);
  }

  {
    using enum mv::PipelineType;

    auto terrainFrustumShaderFile = Helper::readFile ("shaders/terrainFrustumCull.spv");

    vk::ShaderModule terrainFrustumModule = Helper::createShaderModule (terrainFrustumShaderFile);

    vk::PipelineShaderStageCreateInfo cullStageInfo;
    cullStageInfo.stage = vk::ShaderStageFlagBits::eCompute;
    cullStageInfo.module = terrainFrustumModule;
    cullStageInfo.pName = "main";
    cullStageInfo.pSpecializationInfo = nullptr;

    vk::ComputePipelineCreateInfo terrainFrustumComputeInfo;
    terrainFrustumComputeInfo.stage = cullStageInfo;
    terrainFrustumComputeInfo.layout = pipelineLayouts->at (eTerrainCompute);

    auto r = logicalDevice->createComputePipeline (nullptr, terrainFrustumComputeInfo);
    if (r.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create compute pipeline");

    pipelines->insert ({ eTerrainCompute, r.value });

    logicalDevice->destroyShaderModule (terrainFrustumModule);
  }

  return;
}

// Creation of all the graphics pipelines
void
Renderer::prepareGraphicsPipelines (void)
{
  using enum mv::PipelineType;
  using enum mv::RenderPassType;
  auto bindingDescription = Vertex::getBindingDescription ();
  auto attributeDescriptions = Vertex::getAttributeDescriptions ();

  vk::PipelineVertexInputStateCreateInfo viState;
  viState.vertexBindingDescriptionCount = 1;
  viState.pVertexBindingDescriptions = &bindingDescription;
  viState.vertexAttributeDescriptionCount = static_cast<uint32_t> (attributeDescriptions.size ());
  viState.pVertexAttributeDescriptions = attributeDescriptions.data ();

  vk::PipelineInputAssemblyStateCreateInfo iaState;
  iaState.topology = vk::PrimitiveTopology::eTriangleList;

  vk::PipelineInputAssemblyStateCreateInfo debugIaState; // used for dynamic states
  debugIaState.topology = vk::PrimitiveTopology::ePointList;

  vk::PipelineRasterizationStateCreateInfo rsState;
  rsState.depthClampEnable = VK_FALSE;
  rsState.rasterizerDiscardEnable = VK_FALSE;
  rsState.polygonMode = vk::PolygonMode::eLine;
  rsState.cullMode = vk::CullModeFlagBits::eBack;
  rsState.frontFace = vk::FrontFace::eCounterClockwise;
  rsState.depthBiasEnable = VK_FALSE;
  rsState.depthBiasConstantFactor = 0.0f;
  rsState.depthBiasClamp = 0.0f;
  rsState.depthBiasSlopeFactor = 0.0f;
  rsState.lineWidth = 1.0f;

  vk::PipelineColorBlendAttachmentState cbaState;
  cbaState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                            | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  cbaState.blendEnable = VK_FALSE;

  vk::PipelineColorBlendStateCreateInfo cbState;
  cbState.attachmentCount = 1;
  cbState.pAttachments = &cbaState;

  vk::PipelineDepthStencilStateCreateInfo dsState;
  dsState.depthTestEnable = VK_TRUE;
  dsState.depthWriteEnable = VK_TRUE;
  dsState.depthCompareOp = vk::CompareOp::eLessOrEqual;
  dsState.back.compareOp = vk::CompareOp::eAlways;

  vk::Viewport vp;
  vp.x = 0;
  vp.y = 0;
  vp.width = static_cast<float> (swapchain->swapExtent.width);
  vp.height = static_cast<float> (swapchain->swapExtent.height);
  vp.minDepth = 0.0f;
  vp.maxDepth = 1.0f;

  vk::Rect2D sc;
  sc.offset = vk::Offset2D{ 0, 0 };
  sc.extent = swapchain->swapExtent;

  vk::PipelineViewportStateCreateInfo vpState;
  vpState.viewportCount = 1;
  vpState.pViewports = &vp;
  vpState.scissorCount = 1;
  vpState.pScissors = &sc;

  vk::PipelineMultisampleStateCreateInfo msState;
  msState.rasterizationSamples = swapchain->sampleCount;
  msState.sampleShadingEnable = VK_TRUE;
  msState.minSampleShading = 1.0f;

  // Load shaders
  auto vsMVP = Helper::readFile ("shaders/vsMVP.spv");
  auto vsVP = Helper::readFile ("shaders/vsVP.spv");

  auto fsMVPSampler = Helper::readFile ("shaders/fsMVPSampler.spv");
  auto fsMVPNoSampler = Helper::readFile ("shaders/fsMVPNoSampler.spv");

  auto fsVPSampler = Helper::readFile ("shaders/fsVPSampler.spv");
  auto fsVPNoSampler = Helper::readFile ("shaders/fsVPNoSampler.spv");

  // Ensure we have files
  if (vsMVP.empty () || vsVP.empty () || fsMVPSampler.empty () || fsMVPNoSampler.empty ()
      || fsVPSampler.empty () || fsVPNoSampler.empty ())
    throw std::runtime_error ("Failed to load fragment or vertex shader spv files");

  vk::ShaderModule vsModuleMVP = Helper::createShaderModule (vsMVP);
  vk::ShaderModule vsModuleVP = Helper::createShaderModule (vsVP);

  vk::ShaderModule fsModuleMVPSampler = Helper::createShaderModule (fsMVPSampler);
  vk::ShaderModule fsModuleMVPNoSampler = Helper::createShaderModule (fsMVPNoSampler);

  vk::ShaderModule fsModuleVPSampler = Helper::createShaderModule (fsVPSampler);
  vk::ShaderModule fsModuleVPNoSampler = Helper::createShaderModule (fsVPNoSampler);

  /*
      VERTEX SHADER
      MODEL, VIEW, PROJECTION MATRIX
  */
  vk::PipelineShaderStageCreateInfo vsStageInfoMVP;
  vsStageInfoMVP.stage = vk::ShaderStageFlagBits::eVertex;
  vsStageInfoMVP.module = vsModuleMVP;
  vsStageInfoMVP.pName = "main";
  vsStageInfoMVP.pSpecializationInfo = nullptr;

  /*
      VERTEX SHADER
      VIEW, PROJECTION, MATRIX
  */
  vk::PipelineShaderStageCreateInfo vsStageInfoVP;
  vsStageInfoVP.stage = vk::ShaderStageFlagBits::eVertex;
  vsStageInfoVP.module = vsModuleVP;
  vsStageInfoVP.pName = "main";
  vsStageInfoVP.pSpecializationInfo = nullptr;

  /*
      FRAGMENT SHADER
      SAMPLER -- USE MVP VERTEX SHADER
  */
  vk::PipelineShaderStageCreateInfo fsStageInfoMVPSampler;
  fsStageInfoMVPSampler.stage = vk::ShaderStageFlagBits::eFragment;
  fsStageInfoMVPSampler.module = fsModuleMVPSampler;
  fsStageInfoMVPSampler.pName = "main";
  fsStageInfoMVPSampler.pSpecializationInfo = nullptr;

  /*
      FRAGMENT SHADER
      USE MVP VERTEX SHADER
  */
  vk::PipelineShaderStageCreateInfo fsStageInfoMVPNoSampler;
  fsStageInfoMVPNoSampler.stage = vk::ShaderStageFlagBits::eFragment;
  fsStageInfoMVPNoSampler.module = fsModuleMVPNoSampler;
  fsStageInfoMVPNoSampler.pName = "main";
  fsStageInfoMVPNoSampler.pSpecializationInfo = nullptr;

  /*
      FRAGMENT SHADER
      SAMPLER -- USE VP VERTEX SHADER
  */
  vk::PipelineShaderStageCreateInfo fsStageInfoVPSampler;
  fsStageInfoVPSampler.stage = vk::ShaderStageFlagBits::eFragment;
  fsStageInfoVPSampler.module = fsModuleVPSampler;
  fsStageInfoVPSampler.pName = "main";
  fsStageInfoVPSampler.pSpecializationInfo = nullptr;

  /*
       FRAGMENT SHADER
       USE VP VERTEX SHADER
  */
  vk::PipelineShaderStageCreateInfo fsStageInfoVPNoSampler;
  fsStageInfoVPNoSampler.stage = vk::ShaderStageFlagBits::eFragment;
  fsStageInfoVPNoSampler.module = fsModuleVPNoSampler;
  fsStageInfoVPNoSampler.pName = "main";
  fsStageInfoVPNoSampler.pSpecializationInfo = nullptr;

  std::vector<vk::PipelineShaderStageCreateInfo> ssMVPWSampler = {
    vsStageInfoMVP,
    fsStageInfoMVPSampler,
  };
  std::vector<vk::PipelineShaderStageCreateInfo> ssMVPNoSampler = {
    vsStageInfoMVP,
    fsStageInfoMVPNoSampler,
  };
  std::vector<vk::PipelineShaderStageCreateInfo> ssVPWSampler = {
    vsStageInfoVP,
    fsStageInfoVPSampler,
  };
  std::vector<vk::PipelineShaderStageCreateInfo> ssVPNoSampler = {
    vsStageInfoVP,
    fsStageInfoVPNoSampler,
  };

  // Create pipeline for terrain mesh W Sampler
  // V/P uniforms + color, uv, sampler
  vk::GraphicsPipelineCreateInfo plVPSamplerInfo;
  plVPSamplerInfo.renderPass = renderPasses->at (eCore);
  plVPSamplerInfo.layout = pipelineLayouts->at (eVPWSampler);
  plVPSamplerInfo.pInputAssemblyState = &iaState;
  plVPSamplerInfo.pRasterizationState = &rsState;
  plVPSamplerInfo.pColorBlendState = &cbState;
  plVPSamplerInfo.pDepthStencilState = &dsState;
  plVPSamplerInfo.pViewportState = &vpState;
  plVPSamplerInfo.pMultisampleState = &msState;
  plVPSamplerInfo.stageCount = static_cast<uint32_t> (ssVPWSampler.size ());
  plVPSamplerInfo.pStages = ssVPWSampler.data ();
  plVPSamplerInfo.pVertexInputState = &viState;
  plVPSamplerInfo.pDynamicState = nullptr;

  {
    vk::ResultValue result = logicalDevice->createGraphicsPipeline (nullptr, plVPSamplerInfo);
    if (result.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create graphics pipeline for terrain mesh w/sampler");

    pipelines->insert ({ eVPWSampler, result.value });
    if (!pipelines->at (eVPWSampler))
      throw std::runtime_error ("Failed to create pipeline for terrain mesh with sampler");
  }

  // Create pipeline for terrain mesh NO sampler
  // V/P uniforms + color, uv
  vk::GraphicsPipelineCreateInfo plVPNoSamplerInfo;
  plVPNoSamplerInfo.renderPass = renderPasses->at (eCore);
  plVPNoSamplerInfo.layout = pipelineLayouts->at (eVPNoSampler);
  plVPNoSamplerInfo.pInputAssemblyState = &iaState;
  // plVPNoSamplerInfo.pInputAssemblyState = &debugIaState;
  plVPNoSamplerInfo.pRasterizationState = &rsState;
  plVPNoSamplerInfo.pColorBlendState = &cbState;
  plVPNoSamplerInfo.pDepthStencilState = &dsState;
  plVPNoSamplerInfo.pViewportState = &vpState;
  plVPNoSamplerInfo.pMultisampleState = &msState;
  plVPNoSamplerInfo.stageCount = static_cast<uint32_t> (ssVPNoSampler.size ());
  plVPNoSamplerInfo.pStages = ssVPNoSampler.data ();
  plVPNoSamplerInfo.pVertexInputState = &viState;
  plVPNoSamplerInfo.pDynamicState = nullptr;

  {
    vk::ResultValue result = logicalDevice->createGraphicsPipeline (nullptr, plVPNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create graphics pipeline for terrain mesh NO sampler");

    pipelines->insert ({ eVPNoSampler, result.value });
    if (!pipelines->at (eVPNoSampler))
      throw std::runtime_error ("Failed to create pipeline for terrain mesh with no sampler");
  }

  /*
    Dynamic state extension
  */
  std::array<vk::DynamicState, 1> dynStates = {
    vk::DynamicState::ePrimitiveTopologyEXT,
  };
  vk::PipelineDynamicStateCreateInfo dynamicInfo;
  dynamicInfo.dynamicStateCount = static_cast<uint32_t> (dynStates.size ());
  dynamicInfo.pDynamicStates = dynStates.data ();

  // create pipeline WITH sampler
  // M/V/P uniforms + color, uv, sampler
  vk::GraphicsPipelineCreateInfo plMVPSamplerInfo;
  plMVPSamplerInfo.renderPass = renderPasses->at (eCore);
  plMVPSamplerInfo.layout = pipelineLayouts->at (eMVPWSampler);
  plMVPSamplerInfo.pInputAssemblyState = &iaState;
  plMVPSamplerInfo.pRasterizationState = &rsState;
  plMVPSamplerInfo.pColorBlendState = &cbState;
  plMVPSamplerInfo.pDepthStencilState = &dsState;
  plMVPSamplerInfo.pViewportState = &vpState;
  plMVPSamplerInfo.pMultisampleState = &msState;
  plMVPSamplerInfo.stageCount = static_cast<uint32_t> (ssMVPWSampler.size ());
  plMVPSamplerInfo.pStages = ssMVPWSampler.data ();
  plMVPSamplerInfo.pVertexInputState = &viState;
  plMVPSamplerInfo.pDynamicState = nullptr;

  /* Graphics pipeline with sampler -- NO dynamic states */
  {
    vk::ResultValue result = logicalDevice->createGraphicsPipeline (nullptr, plMVPSamplerInfo);
    if (result.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create graphics pipeline with sampler");

    pipelines->insert ({
        eMVPWSampler,
        result.value,
    });
    if (!pipelines->at (eMVPWSampler))
      throw std::runtime_error ("Failed to create generic object pipeline with sampler");
  }

  plMVPSamplerInfo.pInputAssemblyState = &debugIaState;
  plMVPSamplerInfo.pDynamicState = &dynamicInfo;

  /* Graphics pipeline with sampler -- WITH dynamic states */
  {
    vk::ResultValue result = logicalDevice->createGraphicsPipeline (nullptr, plMVPSamplerInfo);
    if (result.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create graphics pipeline with sampler & dynamic states");

    pipelines->insert ({
        eMVPWSamplerDynamic,
        result.value,
    });
    if (!pipelines->at (eMVPWSamplerDynamic))
      throw std::runtime_error ("Failed to create generic object pipeline "
                                "with sampler & dynamic states");
  }

  // Create pipeline with NO sampler
  // M/V/P uniforms + color, uv
  vk::GraphicsPipelineCreateInfo plMVPNoSamplerInfo;
  plMVPNoSamplerInfo.renderPass = renderPasses->at (eCore);
  plMVPNoSamplerInfo.layout = pipelineLayouts->at (eMVPNoSampler);
  plMVPNoSamplerInfo.pInputAssemblyState = &iaState;
  plMVPNoSamplerInfo.pRasterizationState = &rsState;
  plMVPNoSamplerInfo.pColorBlendState = &cbState;
  plMVPNoSamplerInfo.pDepthStencilState = &dsState;
  plMVPNoSamplerInfo.pViewportState = &vpState;
  plMVPNoSamplerInfo.pMultisampleState = &msState;
  plMVPNoSamplerInfo.stageCount = static_cast<uint32_t> (ssMVPNoSampler.size ());
  plMVPNoSamplerInfo.pStages = ssMVPNoSampler.data ();
  plMVPNoSamplerInfo.pVertexInputState = &viState;
  plMVPNoSamplerInfo.pDynamicState = nullptr;

  // Create graphics pipeline NO sampler
  {
    vk::ResultValue result = logicalDevice->createGraphicsPipeline (nullptr, plMVPNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create graphics pipeline with no sampler");

    pipelines->insert ({
        eMVPNoSampler,
        result.value,
    });
    if (!pipelines->at (eMVPNoSampler))
      throw std::runtime_error ("Failed to create generic object pipeline with no sampler");
  }

  // create pipeline no sampler | dynamic state
  plMVPNoSamplerInfo.pInputAssemblyState = &debugIaState;
  plMVPNoSamplerInfo.pDynamicState = &dynamicInfo;

  {
    vk::ResultValue result = logicalDevice->createGraphicsPipeline (nullptr, plMVPNoSamplerInfo);
    if (result.result != vk::Result::eSuccess)
      throw std::runtime_error ("Failed to create graphics pipeline with no "
                                "sampler & dynamic states");

    pipelines->insert ({ eMVPNoSamplerDynamic, result.value });
    if (!pipelines->at (eMVPNoSamplerDynamic))
      throw std::runtime_error ("Failed to create generic object pipeline "
                                "with no sampler & dynamic states");
  }

  logicalDevice->destroyShaderModule (vsModuleMVP);
  logicalDevice->destroyShaderModule (vsModuleVP);

  logicalDevice->destroyShaderModule (fsModuleMVPSampler);
  logicalDevice->destroyShaderModule (fsModuleMVPNoSampler);

  logicalDevice->destroyShaderModule (fsModuleVPSampler);
  logicalDevice->destroyShaderModule (fsModuleVPNoSampler);
  return;
}

void
Renderer::computePass (uint32_t p_ImageIndex)
{
  using enum vk::QueueFlagBits;
  using enum mv::PipelineType;

  vk::CommandBufferBeginInfo beginInfo;

  std::vector<vk::DescriptorSet> toBind = {
    mapHandler->clipSpace->mvBuffer.descriptor,
    mapHandler->vertexData->descriptor, // vertex
    mapHandler->indexData->descriptor,  // index
  };

  auto &[pool, commandBuffers] = commandPools->at (eGraphics);

  commandBuffers.at (p_ImageIndex).begin (beginInfo);

  commandBuffers.at (p_ImageIndex)
      .bindPipeline (vk::PipelineBindPoint::eCompute, pipelines->at (eTerrainCompute));

  commandBuffers.at (p_ImageIndex)
      .bindDescriptorSets (vk::PipelineBindPoint::eCompute,
                           pipelineLayouts->at (eTerrainCompute),
                           0,
                           toBind,
                           nullptr);

  auto gc = (mapHandler.indices.size () / 32) + 1;
  commandBuffers.at (p_ImageIndex).dispatch (static_cast<uint32_t> (gc), 1, 1);

  commandBuffers.at (p_ImageIndex).end ();

  vk::Semaphore signals[] = { semaphores->at (mv::SemaphoreType::eComputeComplete) };
  vk::SubmitInfo submitInfo;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signals;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffers.at (p_ImageIndex);

  auto r = commandQueues->at (vk::QueueFlagBits::eCompute).submit (1, &submitInfo, nullptr);
  if (r != vk::Result::eSuccess)
    {
      throw std::runtime_error ("compute submit failed");
    }
  return;
}

void
Renderer::recordCommandBuffer (uint32_t p_ImageIndex)
{
  using enum mv::PipelineType;
  using enum mv::RenderPassType;
  using enum mv::FramebufferType;
  using enum vk::QueueFlagBits;

  computePass (p_ImageIndex);

  // command buffer begin
  vk::CommandBufferBeginInfo beginInfo;

  // render pass info
  std::array<vk::ClearValue, 3> cls;
  cls[0].color = defaultClearColor;
  cls[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
  cls[2].color = defaultClearColor;

  vk::RenderPassBeginInfo passInfo;
  passInfo.renderPass = renderPasses->at (eCore);
  passInfo.framebuffer = framebuffers->at (eCoreFramebuffer).at (p_ImageIndex);
  passInfo.renderArea.offset.x = 0;
  passInfo.renderArea.offset.y = 0;
  passInfo.renderArea.extent.width = swapchain->swapExtent.width;
  passInfo.renderArea.extent.height = swapchain->swapExtent.height;
  passInfo.clearValueCount = static_cast<uint32_t> (cls.size ());
  passInfo.pClearValues = cls.data ();

  // begin recording
  auto &[pool, commandBuffers] = commandPools->at (eGraphics);
  commandBuffers.at (p_ImageIndex).begin (beginInfo);

  // start render pass
  commandBuffers.at (p_ImageIndex).beginRenderPass (passInfo, vk::SubpassContents::eInline);

  // Render heightmap
  if (mapHandler.isMapLoaded)
    {
      mapHandler.bindBuffer (commandBuffers.at (p_ImageIndex));

      commandBuffers.at (p_ImageIndex)
          .bindPipeline (vk::PipelineBindPoint::eGraphics, pipelines->at (eVPNoSampler));
      currentlyBound = eVPNoSampler;

      std::vector<vk::DescriptorSet> toBind = {
        collectionHandler->projectionUniform->mvBuffer.descriptor,
        collectionHandler->viewUniform->mvBuffer.descriptor,
      };
      commandBuffers.at (p_ImageIndex)
          .bindDescriptorSets (vk::PipelineBindPoint::eGraphics,
                               pipelineLayouts->at (eVPNoSampler),
                               0,
                               toBind,
                               nullptr);

      commandBuffers.at (p_ImageIndex).drawIndexed (mapHandler.indexCount, 1, 0, 0, 0);
    }

  for (auto &model : *collectionHandler->models)
    {
      // for each model select the appropriate pipeline
      if (model.hasTexture && currentlyBound != eMVPWSampler)
        {
          commandBuffers.at (p_ImageIndex)
              .bindPipeline (vk::PipelineBindPoint::eGraphics, pipelines->at (eMVPWSampler));
          currentlyBound = eMVPWSampler;
        }
      else if (!model.hasTexture && currentlyBound != eMVPNoSampler)
        {
          commandBuffers.at (p_ImageIndex)
              .bindPipeline (vk::PipelineBindPoint::eGraphics, pipelines->at (eMVPNoSampler));
          currentlyBound = eMVPNoSampler;
        }

      // Bind vertex & index buffer for model
      model.bindBuffers (commandBuffers.at (p_ImageIndex));

      // For each object
      for (auto &object : *model.objects)
        {
          // Iterate index offsets ;; draw
          for (auto &offset : model.bufferOffsets)
            {
              // { vk::DescriptorSet, Buffer }
              std::vector<vk::DescriptorSet> toBind = {
                collectionHandler->projectionUniform->mvBuffer.descriptor,
                collectionHandler->viewUniform->mvBuffer.descriptor,
                object.uniform.first,
              };
              if (offset.first.second >= 0)
                {
                  toBind.push_back (model.textureDescriptors->at (offset.first.second).first);
                  commandBuffers.at (p_ImageIndex)
                      .bindDescriptorSets (vk::PipelineBindPoint::eGraphics,
                                           pipelineLayouts->at (eMVPWSampler),
                                           0,
                                           toBind,
                                           nullptr);
                }
              else
                {
                  commandBuffers.at (p_ImageIndex)
                      .bindDescriptorSets (vk::PipelineBindPoint::eGraphics,
                                           pipelineLayouts->at (eMVPNoSampler),
                                           0,
                                           toBind,
                                           nullptr);
                }

              // { { vertex offset, Texture index }, { Index start, Index count
              // } }
              commandBuffers.at (p_ImageIndex)
                  .drawIndexed (
                      offset.second.second, 1, offset.second.first, offset.first.first, 0);
            }
        }
    }

  commandBuffers.at (p_ImageIndex).endRenderPass ();

  /*
   IMGUI RENDER
  */
  gui->doRenderPass (renderPasses->at (eImGui),
                     framebuffers->at (eImGuiFramebuffer).at (p_ImageIndex),
                     commandBuffers.at (p_ImageIndex),
                     swapchain->swapExtent);
  /*
    END IMGUI RENDER
  */

  commandBuffers.at (p_ImageIndex).end ();

  return;
}

bool
Renderer::draw (size_t &p_CurrentFrame, uint32_t &p_CurrentImageIndex)
{
  using enum mv::SemaphoreType;
  using enum vk::QueueFlagBits;

  vk::Result res
      = logicalDevice->waitForFences (inFlightFences->at (p_CurrentFrame), VK_TRUE, UINT64_MAX);
  if (res != vk::Result::eSuccess)
    throw std::runtime_error ("Error occurred while waiting for fence");

  vk::Result result = logicalDevice->acquireNextImageKHR (swapchain->swapchain,
                                                          UINT64_MAX,
                                                          semaphores->at (ePresentComplete),
                                                          nullptr,
                                                          &p_CurrentImageIndex);

  switch (result)
    {
      using enum vk::Result;
    case eErrorOutOfDateKHR:
      {
        return false;
      }
    case eSuboptimalKHR:
      {
        return false;
      }
    case eSuccess:
      {
        break;
      }
    default: // unhandled error occurred
      {
        throw std::runtime_error ("Unhandled error case while acquiring a swapchain image for "
                                  "rendering");
      }
    }

  if (waitFences->at (p_CurrentImageIndex))
    {
      vk::Result res = logicalDevice->waitForFences (
          waitFences->at (p_CurrentImageIndex), VK_TRUE, UINT64_MAX);
      if (res != vk::Result::eSuccess)
        throw std::runtime_error ("Error occurred while waiting for fence");
    }

  recordCommandBuffer (p_CurrentImageIndex);

  // mark in use
  waitFences->at (p_CurrentImageIndex) = inFlightFences->at (p_CurrentFrame);

  auto &[pool, commandBuffers] = commandPools->at (vk::QueueFlagBits::eGraphics);

  vk::SubmitInfo submitInfo;
  std::vector<vk::Semaphore> waitSemaphores = {
    semaphores->at (ePresentComplete),
    semaphores->at (eComputeComplete),
  };
  std::vector<vk::PipelineStageFlags> waitStages = {
    vk::PipelineStageFlagBits::eColorAttachmentOutput,
    vk::PipelineStageFlagBits::eColorAttachmentOutput,
  };
  submitInfo.waitSemaphoreCount = static_cast<uint32_t> (waitSemaphores.size ());
  submitInfo.pWaitSemaphores = waitSemaphores.data ();
  submitInfo.pWaitDstStageMask = waitStages.data ();
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffers.at (p_CurrentImageIndex);
  vk::Semaphore renderSemaphores[] = { semaphores->at (eRenderComplete) };
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = renderSemaphores;

  logicalDevice->resetFences (inFlightFences->at (p_CurrentFrame));

  result
      = commandQueues->at (eGraphics).submit (1, &submitInfo, inFlightFences->at (p_CurrentFrame));

  switch (result)
    {
      using enum vk::Result;
    case eErrorOutOfDateKHR:
      {
        return false;
      }
    case eSuboptimalKHR:
      {
        return false;
      }
    case eSuccess:
      {
        break;
      }
    default: // unhandled error occurred
      {
        throw std::runtime_error ("Unhandled error case while submitting queue");
      }
    }

  vk::PresentInfoKHR presentInfo;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = renderSemaphores;
  vk::SwapchainKHR swapchains[] = { swapchain->swapchain };
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &p_CurrentImageIndex;
  presentInfo.pResults = nullptr;

  result = commandQueues->at (eGraphics).presentKHR (&presentInfo);

  switch (result)
    {
      using enum vk::Result;
    case eErrorOutOfDateKHR:
      {
        return false;
      }
    case eSuboptimalKHR:
      {
        return false;
      }
    case eSuccess:
      {
        break;
      }
    default: // unhandled error occurred
      {
        throw std::runtime_error ("Unhandled error case while presenting");
      }
    }
  p_CurrentFrame = (p_CurrentFrame + 1) % MAX_IN_FLIGHT;
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

inline void
Renderer::createPipelineLayout (PipelineLayoutMap &p_LayoutMap,
                                mv::PipelineType p_PipelineType,
                                std::vector<vk::DescriptorSetLayout> &p_Layout)
{
  vk::PipelineLayoutCreateInfo createInfo;
  createInfo.setLayoutCount = static_cast<uint32_t> (p_Layout.size ());
  createInfo.pSetLayouts = p_Layout.data ();

  try
    {
      p_LayoutMap.insert ({ p_PipelineType, logicalDevice->createPipelineLayout (createInfo) });
    }
  catch (vk::SystemError &e)
    {
      throw std::runtime_error ("Vulkan error creating pipeline layout => "
                                + std::string (e.what ()));
    }
  catch (std::exception &e)
    {
      throw std::runtime_error ("Std error creating pipeline layout => " + std::string (e.what ()));
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