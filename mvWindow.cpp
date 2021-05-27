#include "mvWindow.h"

Window::Window (int p_WindowWidth, int p_WindowHeight,
                std::string p_WindowTitle)
{
  windowWidth = p_WindowWidth;
  windowHeight = p_WindowHeight;
  title = p_WindowTitle;

  glfwInit ();

  uint32_t count = 0;
  const char **extensions = glfwGetRequiredInstanceExtensions (&count);
  if (!extensions)
    throw std::runtime_error ("Failed to get required instance extensions");

  // Get our own list of requested extensions
  for (const auto &req : requestedInstanceExtensions)
    {
      instanceExtensions.push_back (req);
    }

  // Get GLFW requested extensions
  std::vector<std::string> glfwRequested;
  for (uint32_t i = 0; i < count; i++)
    {
      glfwRequested.push_back (extensions[i]);
    }

  // temp container for missing extensions
  std::vector<std::string> tmp;

  // iterate glfw requested
  for (const auto &glfw_req : glfwRequested)
    {
      bool found = false;

      // iterate already requested list
      for (const auto &extensionName : instanceExtensions)
        {
          if (glfw_req == extensionName)
            {
              found = true;
            }
        }

      // if not found, add to final list
      if (!found)
        instanceExtensions.push_back (glfw_req);
    }

  /*
      Create window, windowed fullscreen
  */
  auto monitor = glfwGetPrimaryMonitor ();
  const GLFWvidmode *videoMode = glfwGetVideoMode (monitor);

  glfwWindowHint (GLFW_RED_BITS, videoMode->redBits);
  glfwWindowHint (GLFW_GREEN_BITS, videoMode->greenBits);
  glfwWindowHint (GLFW_BLUE_BITS, videoMode->blueBits);
  glfwWindowHint (GLFW_REFRESH_RATE, videoMode->refreshRate);
  glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow (videoMode->width, videoMode->height,
                             title.c_str (), monitor, nullptr);
  if (!window)
    throw std::runtime_error ("Failed to create window");

  auto rawMouseSupport = glfwRawMouseMotionSupported ();

  if (!rawMouseSupport)
    throw std::runtime_error ("Raw mouse motion not supported");
  // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  // glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, true);

  // Set keyboard callback
  glfwSetKeyCallback (window, keyCallback);

  // Set mouse motion callback
  glfwSetCursorPosCallback (window, mouseMotionCallback);

  // Set mouse button callback
  glfwSetMouseButtonCallback (window, mouseButtonCallback);

  // Set mouse wheel callback
  glfwSetScrollCallback (window, mouseScrollCallback);

  // For imgui circular callback when recreating swap
  glfwSetCharCallback (window, NULL);

  // Add our base app class to glfw window for callbacks
  glfwSetWindowUserPointer (window, this);
  return;
}

Window::~Window ()
{
  // ensure gpu not using any resources
  logicalDevice.waitIdle ();

  swapchain.cleanup (instance, logicalDevice);

  // cleanup command buffers
  if (!commandBuffers.empty () && commandPool)
    {
      logicalDevice.freeCommandBuffers (commandPool, commandBuffers);
    }

  // cleanup sync objects
  if (!inFlightFences.empty ())
    {
      for (auto &fence : inFlightFences)
        {
          if (fence)
            {
              logicalDevice.destroyFence (fence, nullptr);
            }
        }
    }
  if (semaphores.presentComplete)
    logicalDevice.destroySemaphore (semaphores.presentComplete, nullptr);
  if (semaphores.renderComplete)
    logicalDevice.destroySemaphore (semaphores.renderComplete, nullptr);

  if (depthStencil.image)
    {
      logicalDevice.destroyImage (depthStencil.image, nullptr);
    }
  if (depthStencil.view)
    {
      logicalDevice.destroyImageView (depthStencil.view, nullptr);
    }
  if (depthStencil.mem)
    {
      logicalDevice.freeMemory (depthStencil.mem, nullptr);
    }

  if (!renderPasses.empty ())
    {
      for (auto &pass : renderPasses)
        {
          if (pass.second)
            logicalDevice.destroyRenderPass (pass.second, nullptr);
        }
    }

  // core engine render framebuffers
  if (!coreFramebuffers.empty ())
    {
      for (auto &buffer : coreFramebuffers)
        {
          if (buffer)
            {
              logicalDevice.destroyFramebuffer (buffer, nullptr);
            }
        }
    }

  // ImGui framebuffers
  if (!guiFramebuffers.empty ())
    {
      for (auto &buffer : guiFramebuffers)
        {
          if (buffer)
            {
              logicalDevice.destroyFramebuffer (buffer, nullptr);
            }
        }
    }

  if (commandPool)
    logicalDevice.destroyCommandPool (commandPool);

  if (logicalDevice)
    logicalDevice.destroy ();

#ifndef NDEBUG
  pfn_vkDestroyDebugUtilsMessengerEXT (
      instance,
      static_cast<VkDebugUtilsMessengerEXT> (vulkanHandleDebugCallback),
      nullptr);
#endif

  if (instance)
    instance.destroy ();

  if (window)
    glfwDestroyWindow (window);
  glfwTerminate ();
  return;
}

void
Window::prepare (void)
{
  // creates...
  // physical device
  // logical device
  // swapchain surface
  initVulkan ();

  std::cout << "[+] Initializing swapchain handler\n";
  swapchain.init (window, instance, physicalDevice);

  // get depth format
  swapchain.depthFormat = getSupportedDepthFormat ();

  swapchain.create (physicalDevice, logicalDevice, windowWidth, windowHeight);

  createCommandBuffers ();

  createSynchronizationPrimitives ();

  setupDepthStencil ();

  setupRenderPass ();

  // create_pipeline_cache();

  setupFramebuffer ();

  // load device extension functions
  pfn_vkCmdSetPrimitiveTopology
      = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT> (
          vkGetInstanceProcAddr (instance, "vkCmdSetPrimitiveTopologyEXT"));
  if (!pfn_vkCmdSetPrimitiveTopology)
    throw std::runtime_error (
        "Failed to load extended dynamic state extensions");
  return;
}

void
Window::initVulkan (void)
{
  // creates vulkan instance with specified instance extensions/layers
  createInstance ();

  // Load debug utility functions for persistent validation error reporting

  std::vector<vk::PhysicalDevice> physicalDevices
      = instance.enumeratePhysicalDevices ();

  if (physicalDevices.size () < 1)
    throw std::runtime_error ("No physical devices found");

  std::cout << "[+] Fetching physical device " << physicalDevices.at (0)
            << "\n";
  // Select device
  physicalDevice = physicalDevices.at (0);

  // resize devices container
  physicalDevices.erase (physicalDevices.begin ());

  // clang-format off
    physicalProperties                      = physicalDevice.getProperties();
    physicalMemoryProperties                = physicalDevice.getMemoryProperties();
    queueFamilyProperties                   = physicalDevice.getQueueFamilyProperties();
    physicalFeatures                        = physicalDevice.getFeatures();
    physicalFeatures2                       = physicalDevice.getFeatures2();
    physicalDeviceExtensions                = physicalDevice.enumerateDeviceExtensionProperties();
    extendedFeatures.extendedDynamicState   = VK_TRUE;
    physicalFeatures2.pNext                 = &extendedFeatures;
  // clang-format on

  if (queueFamilyProperties.empty ())
    throw std::runtime_error ("Failed to find any queue families");

  if (physicalDeviceExtensions.empty ())
    if (!requestedDeviceExtensions.empty ())
      throw std::runtime_error ("Failed to find device extensions");

  // Add requestedDeviceExtensions to class member
  // requestedLogicalDeviceExtensions
  for (const auto &requested : requestedDeviceExtensions)
    {
      requestedLogicalDeviceExtensions.push_back (requested);
    }

  auto checkIfSupported = [&, this] (const std::string requested_ext) {
    for (const auto &supportedExtension : physicalDeviceExtensions)
      {
        if (strcmp (supportedExtension.extensionName, requested_ext.c_str ())
            == 0)
          {
            return true;
          }
      }
    std::cout << "Failed to find extension => " << requested_ext << "\n";
    return false;
  };

  bool haveAllExtensions = std::all_of (
      requestedLogicalDeviceExtensions.begin (),
      requestedLogicalDeviceExtensions.end (), checkIfSupported);

  if (!haveAllExtensions)
    throw std::runtime_error (
        "Failed to find all requested device extensions");

  std::vector<std::string> tmp;
  for (const auto &extensionName : requestedDeviceExtensions)
    {
      tmp.push_back (extensionName);
    }

  // create logical device & graphics queue
  createLogicalDevice ();

  // get format
  // depthFormat = getSupportedDepthFormat(*physicalDevice);

  // no longer pass references to swapchain, pass reference on per function
  // basis now swapchain->map(std::weak_ptr<vk::Instance>(instance),
  //                std::weak_ptr<vk::PhysicalDevice>(physical_device),
  //                std::weak_ptr<Device>(mvDevice));

  // Create synchronization objects
  vk::SemaphoreCreateInfo semaphoreInfo;
  semaphores.presentComplete = logicalDevice.createSemaphore (semaphoreInfo);
  semaphores.renderComplete = logicalDevice.createSemaphore (semaphoreInfo);

  return;
}

void
Window::createInstance (void)
{
  // Ensure we have validation layers
#ifndef NDEBUG
  checkValidationSupport ();
#endif

  // Ensure we have all requested instance extensions
  checkInstanceExt ();

  vk::ApplicationInfo appInfo;
  appInfo.pApplicationName = "Bloody Day";
  appInfo.applicationVersion = VK_MAKE_VERSION (1, 0, 0);
  appInfo.pEngineName = "Moogin";
  appInfo.engineVersion = VK_MAKE_VERSION (1, 0, 0);
  appInfo.apiVersion = VK_MAKE_VERSION (1, 2, 0);

/* If debugging enabled */
#ifndef NDEBUG
  vk::DebugUtilsMessengerCreateInfoEXT debuggerSettings;
  debuggerSettings.messageSeverity
      = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
  debuggerSettings.messageType
      = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  debuggerSettings.pfnUserCallback = debug_message_processor;
  debuggerSettings.pUserData = nullptr;

  // convert string request to const char*
  std::vector<const char *> req_layers;
  for (auto &layerName : requestedValidationLayers)
    {
      req_layers.push_back (layerName);
    }
  std::vector<const char *> req_inst_ext;
  for (auto &ext : instanceExtensions)
    {
      req_inst_ext.push_back (ext.c_str ());
    }

  vk::InstanceCreateInfo createInfo;
  createInfo.pNext = &debuggerSettings;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledLayerCount = static_cast<uint32_t> (req_layers.size ());
  createInfo.ppEnabledLayerNames = req_layers.data ();
  createInfo.enabledExtensionCount
      = static_cast<uint32_t> (req_inst_ext.size ());
  createInfo.ppEnabledExtensionNames = req_inst_ext.data ();
#endif
#ifdef NDEBUG /* Debugging disabled */
  std::vector<const char *> req_inst_ext;
  for (auto &ext : instanceExtensions)
    {
      req_inst_ext.push_back (ext.c_str ());
    }

  vk::InstanceCreateInfo createInfo;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount
      = static_cast<uint32_t> (req_inst_ext.size ());
  createInfo.ppEnabledExtensionNames = req_inst_ext.data ();
#endif

  std::cout << "[+] Creating instance\n";
  instance = vk::createInstance (createInfo);
  // double check instance(prob a triple check at this point)
  if (!instance)
    throw std::runtime_error ("Failed to create vulkan instance");

#ifndef NDEBUG
  // Load debug util function
  pfn_vkCreateDebugUtilsMessengerEXT
      = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT> (
          vkGetInstanceProcAddr (instance, "vkCreateDebugUtilsMessengerEXT"));
  if (!pfn_vkCreateDebugUtilsMessengerEXT)
    throw std::runtime_error (
        "Failed to load debug messenger creation function");

  pfn_vkDestroyDebugUtilsMessengerEXT
      = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT> (
          vkGetInstanceProcAddr (instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (!pfn_vkDestroyDebugUtilsMessengerEXT)
    throw std::runtime_error (
        "Failed to load debug messenger destroy function");

  vk::DebugUtilsMessengerCreateInfoEXT debugCallbackInfo;
  debugCallbackInfo.messageSeverity
      = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
        | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
  debugCallbackInfo.messageType
      = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
        | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
        | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  debugCallbackInfo.pfnUserCallback = generalDebugCallback;

  VkResult result = pfn_vkCreateDebugUtilsMessengerEXT (
      instance,
      reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT *> (
          &debugCallbackInfo),
      nullptr,
      reinterpret_cast<VkDebugUtilsMessengerEXT *> (
          &vulkanHandleDebugCallback));
  if (result != VK_SUCCESS)
    throw std::runtime_error ("Failed to create debug utils messenger");
#endif

  return;
}

void
Window::createLogicalDevice (void)
{
  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

  constexpr float defaultQueuePriority = 0.0f;

  // Look for graphics queue
  queueIdx.graphics = getQueueFamilyIndex (vk::QueueFlagBits::eGraphics);

  vk::DeviceQueueCreateInfo queueInfo;
  queueInfo.queueCount = 1;
  queueInfo.queueFamilyIndex = queueIdx.graphics;
  queueInfo.pQueuePriorities = &defaultQueuePriority;

  queueCreateInfos.push_back (queueInfo);

  std::vector<const char *> enabledExtensions;
  for (const auto &req : requestedLogicalDeviceExtensions)
    {
      enabledExtensions.push_back (req.c_str ());
    }
  vk::DeviceCreateInfo deviceCreateInfo;
  deviceCreateInfo.pNext = &physicalFeatures2;
  deviceCreateInfo.queueCreateInfoCount
      = static_cast<uint32_t> (queueCreateInfos.size ());
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data ();

  // if pnext is phys features2, must be nullptr
  // device_create_info.pEnabledFeatures = &physical_features;

  deviceCreateInfo.enabledExtensionCount
      = static_cast<uint32_t> (enabledExtensions.size ());
  deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data ();

  // create logical device
  logicalDevice = physicalDevice.createDevice (deviceCreateInfo);

  // create command pool with graphics queue
  commandPool = createCommandPool (queueIdx.graphics);

  // retreive graphics queue
  graphicsQueue = logicalDevice.getQueue (queueIdx.graphics, 0);
  return;
}

void
Window::createCommandBuffers (void)
{
  commandBuffers.resize (swapchain.buffers.size ());

  vk::CommandBufferAllocateInfo allocInfo;
  allocInfo.commandPool = commandPool;
  allocInfo.level = vk::CommandBufferLevel::ePrimary;
  allocInfo.commandBufferCount
      = static_cast<uint32_t> (commandBuffers.size ());

  commandBuffers = logicalDevice.allocateCommandBuffers (allocInfo);

  if (commandBuffers.size () < 1)
    throw std::runtime_error ("Failed to allocate command buffers");
  return;
}

void
Window::createSynchronizationPrimitives (void)
{
  vk::FenceCreateInfo fenceInfo;
  fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

  waitFences.resize (swapchain.buffers.size (), nullptr);
  inFlightFences.resize (MAX_IN_FLIGHT);

  for (auto &fence : inFlightFences)
    {
      fence = logicalDevice.createFence (fenceInfo);
    }
  return;
}

void
Window::setupDepthStencil (void)
{
  vk::ImageCreateInfo imageInfo;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.format = swapchain.depthFormat;
  imageInfo.extent = vk::Extent3D{ windowWidth, windowHeight, 1 };
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = vk::SampleCountFlagBits::e1;
  imageInfo.tiling = vk::ImageTiling::eOptimal;
  imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

  // create depth stencil testing image
  depthStencil.image = logicalDevice.createImage (imageInfo);

  // Allocate memory for image
  vk::MemoryRequirements memReq;
  memReq = logicalDevice.getImageMemoryRequirements (depthStencil.image);

  vk::MemoryAllocateInfo allocInfo;
  allocInfo.allocationSize = memReq.size;
  allocInfo.memoryTypeIndex = getMemoryType (
      memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

  // allocate depth stencil image memory
  depthStencil.mem = logicalDevice.allocateMemory (allocInfo);

  // bind image and memory
  logicalDevice.bindImageMemory (depthStencil.image, depthStencil.mem, 0);

  // Create view into depth stencil testing image
  vk::ImageViewCreateInfo viewInfo;
  viewInfo.viewType = vk::ImageViewType::e2D;
  viewInfo.image = depthStencil.image;
  viewInfo.format = swapchain.depthFormat;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;
  viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

  // if physical device supports high enough format add stenciling
  if (swapchain.depthFormat >= vk::Format::eD16UnormS8Uint)
    {
      viewInfo.subresourceRange.aspectMask
          |= vk::ImageAspectFlagBits::eStencil;
    }

  depthStencil.view = logicalDevice.createImageView (viewInfo);
  return;
}

void
Window::setupRenderPass (void)
{
  std::array<vk::AttachmentDescription, 2> coreAttachments;
  // Color attachment
  coreAttachments[0].format = swapchain.colorFormat;
  coreAttachments[0].samples = vk::SampleCountFlagBits::e1;
  coreAttachments[0].loadOp = vk::AttachmentLoadOp::eClear;
  coreAttachments[0].storeOp = vk::AttachmentStoreOp::eStore;
  coreAttachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  coreAttachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[0].initialLayout = vk::ImageLayout::eUndefined;
  coreAttachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

  // Depth attachment
  coreAttachments[1].format = swapchain.depthFormat;
  coreAttachments[1].samples = vk::SampleCountFlagBits::e1;
  coreAttachments[1].loadOp = vk::AttachmentLoadOp::eClear;
  coreAttachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  coreAttachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
  coreAttachments[1].initialLayout = vk::ImageLayout::eUndefined;
  coreAttachments[1].finalLayout
      = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  /*
      Core render references
  */
  vk::AttachmentReference colorRef;
  colorRef.attachment = 0;
  colorRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::AttachmentReference depthRef;
  depthRef.attachment = 1;
  depthRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

  // color attachment and depth testing subpass
  vk::SubpassDescription corePass;
  corePass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  corePass.colorAttachmentCount = 1;
  corePass.pColorAttachments = &colorRef;
  corePass.pDepthStencilAttachment = &depthRef;

  std::vector<vk::SubpassDescription> coreSubpasses = { corePass };

  std::array<vk::SubpassDependency, 1> coreDependencies;
  // Color + depth subpass
  coreDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
  coreDependencies[0].dstSubpass = 0;
  coreDependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
  coreDependencies[0].dstStageMask
      = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  coreDependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
  coreDependencies[0].dstAccessMask
      = vk::AccessFlagBits::eColorAttachmentRead
        | vk::AccessFlagBits::eColorAttachmentWrite;
  coreDependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

  vk::RenderPassCreateInfo coreRenderPassInfo;
  coreRenderPassInfo.attachmentCount
      = static_cast<uint32_t> (coreAttachments.size ());
  coreRenderPassInfo.pAttachments = coreAttachments.data ();
  coreRenderPassInfo.subpassCount
      = static_cast<uint32_t> (coreSubpasses.size ());
  coreRenderPassInfo.pSubpasses = coreSubpasses.data ();
  coreRenderPassInfo.dependencyCount
      = static_cast<uint32_t> (coreDependencies.size ());
  coreRenderPassInfo.pDependencies = coreDependencies.data ();

  vk::RenderPass tempCore
      = logicalDevice.createRenderPass (coreRenderPassInfo);

  renderPasses.insert ({
      eCore,
      std::move (tempCore),
  });
  return;
}

// TODO
// Re add call to this method
// void MWindow::create_pipeline_cache(void)
// {
//     vk::PipelineCacheCreateInfo pcinfo;
//     pipeline_cache =
//     std::make_shared<vk::PipelineCache>(logical_device->createPipelineCache(pcinfo));
//     return;
// }

void
Window::setupFramebuffer (void)
{
  // Attachments for core engine rendering
  std::array<vk::ImageView, 2> coreAttachments;

  if (!depthStencil.view)
    throw std::runtime_error ("Depth stencil view is nullptr");

  // each frame buffer uses same depth image
  coreAttachments[1] = depthStencil.view;

  // core engine render will use color attachment buffer & depth buffer
  vk::FramebufferCreateInfo coreFrameInfo;
  coreFrameInfo.renderPass = renderPasses.at (eCore);
  coreFrameInfo.attachmentCount
      = static_cast<uint32_t> (coreAttachments.size ());
  coreFrameInfo.pAttachments = coreAttachments.data ();
  coreFrameInfo.width = swapchain.swapExtent.width;
  coreFrameInfo.height = swapchain.swapExtent.height;
  coreFrameInfo.layers = 1;

  // Framebuffer per swap image
  coreFramebuffers.resize (static_cast<uint32_t> (swapchain.buffers.size ()));
  for (size_t i = 0; i < coreFramebuffers.size (); i++)
    {
      if (!swapchain.buffers.at (i).image)
        throw std::runtime_error ("Swapchain buffer at index "
                                  + std::to_string (i) + " has nullptr image");
      if (!swapchain.buffers.at (i).view)
        throw std::runtime_error ("Swapchain buffer at index "
                                  + std::to_string (i) + " has nullptr view");
      // Assign each swapchain image to a frame buffer
      coreAttachments[0] = swapchain.buffers.at (i).view;

      coreFramebuffers.at (i)
          = logicalDevice.createFramebuffer (coreFrameInfo);
    }
  return;
}

void
Window::checkValidationSupport (void)
{
  std::vector<vk::LayerProperties> enumeratedInstLayers
      = vk::enumerateInstanceLayerProperties ();

  if (enumeratedInstLayers.size () == 0 && !requestedValidationLayers.empty ())
    throw std::runtime_error ("No supported validation layers found");

  // look for missing requested layers
  std::string prelude = "The following instance layers were not found...\n";
  std::string failed;
  for (const auto &reqLayerName : requestedValidationLayers)
    {
      bool match = false;
      for (const auto &layer : enumeratedInstLayers)
        {
          if (strcmp (reqLayerName, layer.layerName) == 0)
            {
              match = true;
              break;
            }
        }
      if (!match)
        {
          failed += reqLayerName;
          failed += "\n";
        }
    }

  // report missing layers if necessary
  if (!failed.empty ())
    {
      throw std::runtime_error (prelude + failed);
    }
  else
    {
      std::cout << "System supports all requested validation layers\n";
    }
  return;
}

void
Window::checkInstanceExt (void)
{
  std::vector<vk::ExtensionProperties> enumeratedInstExtensions
      = vk::enumerateInstanceExtensionProperties ();

  // use f_req vector for instance extensions
  if (enumeratedInstExtensions.size () < 1
      && !requestedInstanceExtensions.empty ())
    throw std::runtime_error ("No instance extensions found");

  std::string prelude
      = "The following instance extensions were not found...\n";
  std::string failed;
  for (const auto &reqInstExtensionName : requestedInstanceExtensions)
    {
      bool match = false;
      for (const auto &availableExtension : enumeratedInstExtensions)
        {
          if (strcmp (reqInstExtensionName, availableExtension.extensionName)
              == 0)
            {
              match = true;
              break;
            }
        }
      if (!match)
        {
          failed += reqInstExtensionName;
          failed += "\n";
        }
    }

  if (!failed.empty ())
    {
      throw std::runtime_error (prelude + failed);
    }

  return;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_message_processor (
    VkDebugUtilsMessageSeverityFlagBitsEXT p_MessageSeverity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT p_MessageType,
    const VkDebugUtilsMessengerCallbackDataEXT *p_CallbackData,
    [[maybe_unused]] void *p_UserData)
{
  if (p_MessageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
      std::ostringstream oss;
      oss << "Vulkan Performance Validation => "
          << p_CallbackData->messageIdNumber << ", "
          << p_CallbackData->pMessageIdName << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
    }
  if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
      std::ostringstream oss;
      oss << std::endl
          << "Warning: " << p_CallbackData->messageIdNumber << ", "
          << p_CallbackData->pMessageIdName << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
    }
  // else if (p_MessageSeverity &
  // VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
  // {
  //     // Disabled by the impossible statement
  //     std::ostringstream oss;
  //     oss << std::endl
  //         << "Verbose message : " << p_CallbackData->messageIdNumber << ",
  //         " << p_CallbackData->pMessageIdName
  //         << std::endl
  //         << p_CallbackData->pMessage << "\n\n";
  //     std::cout << oss.str();
  // }
  else if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
      std::ostringstream oss;
      oss << std::endl
          << "Error: " << p_CallbackData->messageIdNumber << ", "
          << p_CallbackData->pMessageIdName << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
    }
  // else if (p_MessageSeverity &
  // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
  // {
  //     std::ostringstream oss;
  //     oss << std::endl
  //         << "Info: " << p_CallbackData->messageIdNumber << ", " <<
  //         p_CallbackData->pMessageIdName << "\n"
  //         << p_CallbackData->pMessage << "\n\n";
  //     std::cout << oss.str();
  // }
  return false;
}
VKAPI_ATTR
VkBool32 VKAPI_CALL
generalDebugCallback (
    VkDebugUtilsMessageSeverityFlagBitsEXT p_MessageSeverity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT p_MessageType,
    const VkDebugUtilsMessengerCallbackDataEXT *p_CallbackData,
    [[maybe_unused]] void *p_UserData)
{
  if (p_MessageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
      std::ostringstream oss;
      oss << "Vulkan Performance Validation => "
          << p_CallbackData->messageIdNumber << ", "
          << p_CallbackData->pMessageIdName << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
    }
  if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
      std::ostringstream oss;
      oss << std::endl
          << "Warning: " << p_CallbackData->messageIdNumber << ", "
          << p_CallbackData->pMessageIdName << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
    }
  if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
      std::ostringstream oss;
      oss << std::endl
          << "Error: " << p_CallbackData->messageIdNumber << ", "
          << p_CallbackData->pMessageIdName << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
    }
  return false;
}

std::vector<char>
Window::readFile (std::string p_Filename)
{
  size_t fileSize;
  std::ifstream file;
  std::vector<char> buffer;

  // check if file exists
  try
    {
      if (!std::filesystem::exists (p_Filename))
        throw std::runtime_error ("Shader file " + p_Filename
                                  + " does not exist");

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
      throw std::runtime_error (
          "File reading operation returned empty buffer :: shaders?");
    }

  return buffer;
}

vk::ShaderModule
Window::createShaderModule (const std::vector<char> &p_ShaderCharBuffer)
{
  vk::ShaderModuleCreateInfo moduleInfo;
  moduleInfo.codeSize = p_ShaderCharBuffer.size ();
  moduleInfo.pCode
      = reinterpret_cast<const uint32_t *> (p_ShaderCharBuffer.data ());

  vk::ShaderModule module = logicalDevice.createShaderModule (moduleInfo);

  return module;
}

vk::CommandPool
Window::createCommandPool (uint32_t p_QueueIndex,
                           vk::CommandPoolCreateFlags p_CreateFlags)
{
  vk::CommandPoolCreateInfo poolInfo;
  poolInfo.flags = p_CreateFlags;
  poolInfo.queueFamilyIndex = p_QueueIndex;

  return logicalDevice.createCommandPool (poolInfo);
}

vk::Format
Window::getSupportedDepthFormat (void) const
{
  std::vector<vk::Format> depthFormats = {
    vk::Format::eD32SfloatS8Uint, vk::Format::eD32Sfloat,
    vk::Format::eD24UnormS8Uint,  vk::Format::eD16UnormS8Uint,
    vk::Format::eD16Unorm,
  };

  for (auto &format : depthFormats)
    {
      vk::FormatProperties formatProperties
          = physicalDevice.getFormatProperties (format);
      if (formatProperties.optimalTilingFeatures
          & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
        {
          if (!(formatProperties.optimalTilingFeatures
                & vk::FormatFeatureFlagBits::eSampledImage))
            continue;
        }
      return format;
    }
  throw std::runtime_error ("Failed to find good format");
}

uint32_t
Window::getMemoryType (uint32_t p_MemoryTypeBits,
                       vk::MemoryPropertyFlags p_MemoryProperties,
                       vk::Bool32 *p_IsMemoryTypeFound) const
{
  for (uint32_t i = 0; i < physicalMemoryProperties.memoryTypeCount; i++)
    {
      if ((p_MemoryTypeBits & 1) == 1)
        {
          if ((physicalMemoryProperties.memoryTypes[i].propertyFlags
               & p_MemoryProperties))
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
Window::getQueueFamilyIndex (vk::QueueFlagBits p_QueueFlagBits) const
{
  for (const auto &queueProperty : queueFamilyProperties)
    {
      if ((queueProperty.queueFlags & p_QueueFlagBits))
        {
          return (&queueProperty - &queueFamilyProperties[0]);
        }
    }

  throw std::runtime_error ("Could not find requested queue family");
}