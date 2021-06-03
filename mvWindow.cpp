#include "mvWindow.h"

// for msaa image method
#include "mvImage.h"

#include "mvHelper.h"

Window::Window (int p_WindowWidth, int p_WindowHeight, std::string p_WindowTitle)
{
  windowWidth = p_WindowWidth;
  windowHeight = p_WindowHeight;
  title = p_WindowTitle;

  instance = std::make_unique<vk::Instance> ();
  physicalDevice = std::make_unique<vk::PhysicalDevice> ();
  logicalDevice = std::make_unique<vk::Device> ();
  swapchain = std::make_unique<Swap> ();

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

  // Create window, windowed fullscreen
  auto monitor = glfwGetPrimaryMonitor ();
  const GLFWvidmode *videoMode = glfwGetVideoMode (monitor);

  glfwWindowHint (GLFW_RED_BITS, videoMode->redBits);
  glfwWindowHint (GLFW_GREEN_BITS, videoMode->greenBits);
  glfwWindowHint (GLFW_BLUE_BITS, videoMode->blueBits);
  glfwWindowHint (GLFW_REFRESH_RATE, videoMode->refreshRate);
  glfwWindowHint (GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
  window = glfwCreateWindow (videoMode->width, videoMode->height, title.c_str (), monitor, nullptr);
  if (!window)
    throw std::runtime_error ("Failed to create window");
  return;
}

Window::~Window ()
{
  // ensure gpu not using any resources
  if (!logicalDevice)
    return;

  logicalDevice->waitIdle ();

  if (swapchain)
    swapchain->cleanup (*instance, *logicalDevice);

  if (logicalDevice)
    logicalDevice->destroy ();

#ifndef NDEBUG
  if (instance)
    pfn_vkDestroyDebugUtilsMessengerEXT (
        *instance, static_cast<VkDebugUtilsMessengerEXT> (vulkanHandleDebugCallback), nullptr);
#endif

  if (instance)
    instance->destroy ();

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
  // surface
  initVulkan ();

  std::cout << "[+] Initializing swapchain handler\n";
  swapchain->init (window, *instance, *physicalDevice);

  swapchain->create (*physicalDevice, *logicalDevice, windowWidth, windowHeight);

  // load device extension functions
  pfn_vkCmdSetPrimitiveTopology = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT> (
      vkGetInstanceProcAddr (*instance, "vkCmdSetPrimitiveTopologyEXT"));
  if (!pfn_vkCmdSetPrimitiveTopology)
    throw std::runtime_error ("Failed to load extended dynamic state extensions");
  return;
}

void
Window::initVulkan (void)
{
  // creates vulkan instance with specified instance extensions/layers
  createInstance ();

  // Load debug utility functions for persistent validation error reporting

  std::vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices ();

  if (physicalDevices.size () < 1)
    throw std::runtime_error ("No physical devices found");

  std::cout << "[+] Fetching physical device " << physicalDevices.at (0) << "\n";
  // Select device
  *physicalDevice = physicalDevices.at (0);

  // clang-format off
    physicalProperties                      = physicalDevice->getProperties();
    physicalMemoryProperties                = physicalDevice->getMemoryProperties();
    queueFamilyProperties                   = physicalDevice->getQueueFamilyProperties();
    physicalFeatures                        = physicalDevice->getFeatures();
    physicalFeatures2                       = physicalDevice->getFeatures2();
    physicalDeviceExtensions                = physicalDevice->enumerateDeviceExtensionProperties();
    extendedFeatures.extendedDynamicState   = VK_TRUE;
    physicalFeatures.sampleRateShading      = VK_TRUE;
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
        if (strcmp (supportedExtension.extensionName, requested_ext.c_str ()) == 0)
          {
            return true;
          }
      }
    std::cout << "Failed to find extension => " << requested_ext << "\n";
    return false;
  };

  bool haveAllExtensions = std::all_of (requestedLogicalDeviceExtensions.begin (),
                                        requestedLogicalDeviceExtensions.end (),
                                        checkIfSupported);

  if (!haveAllExtensions)
    throw std::runtime_error ("Failed to find all requested device extensions");

  std::vector<std::string> tmp;
  for (const auto &extensionName : requestedDeviceExtensions)
    {
      tmp.push_back (extensionName);
    }

  // create logical device & graphics queue
  createLogicalDevice (vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eGraphics);

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
  debuggerSettings.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose
                                     | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning
                                     | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
  debuggerSettings.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                 | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                                 | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  debuggerSettings.pfnUserCallback = initialDebugProcessor;
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
  createInfo.enabledExtensionCount = static_cast<uint32_t> (req_inst_ext.size ());
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
  createInfo.enabledExtensionCount = static_cast<uint32_t> (req_inst_ext.size ());
  createInfo.ppEnabledExtensionNames = req_inst_ext.data ();
#endif

  std::cout << "[+] Creating instance\n";
  *instance = vk::createInstance (createInfo);
  // double check instance(prob a triple check at this point)
  if (!instance)
    throw std::runtime_error ("Failed to create vulkan instance");

#ifndef NDEBUG
  // Load debug util function
  pfn_vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT> (
      vkGetInstanceProcAddr (*instance, "vkCreateDebugUtilsMessengerEXT"));
  if (!pfn_vkCreateDebugUtilsMessengerEXT)
    throw std::runtime_error ("Failed to load debug messenger creation function");

  pfn_vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT> (
      vkGetInstanceProcAddr (*instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (!pfn_vkDestroyDebugUtilsMessengerEXT)
    throw std::runtime_error ("Failed to load debug messenger destroy function");

  vk::DebugUtilsMessengerCreateInfoEXT debugCallbackInfo;
  debugCallbackInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError
                                      | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
                                      | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
  debugCallbackInfo.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
                                  | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance
                                  | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
  debugCallbackInfo.pfnUserCallback = generalDebugCallback;

  VkResult result = pfn_vkCreateDebugUtilsMessengerEXT (
      *instance,
      reinterpret_cast<VkDebugUtilsMessengerCreateInfoEXT *> (&debugCallbackInfo),
      nullptr,
      reinterpret_cast<VkDebugUtilsMessengerEXT *> (&vulkanHandleDebugCallback));
  if (result != VK_SUCCESS)
    throw std::runtime_error ("Failed to create debug utils messenger");
#endif

  return;
}

void
Window::createLogicalDevice (vk::QueueFlags p_RequestedQueueFlags)
{
  std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

  constexpr float defaultQueuePriority = 0.0f;

  if (p_RequestedQueueFlags & vk::QueueFlagBits::eGraphics)
    {
      queueIdx.graphics = Helper::getQueueFamilyIndex (vk::QueueFlagBits::eGraphics);
      vk::DeviceQueueCreateInfo graphicsQueueCreateInfo;
      graphicsQueueCreateInfo.queueCount = 1;
      graphicsQueueCreateInfo.queueFamilyIndex = queueIdx.graphics;
      graphicsQueueCreateInfo.pQueuePriorities = &defaultQueuePriority;
      queueCreateInfos.push_back (graphicsQueueCreateInfo);
    }
  if (p_RequestedQueueFlags & vk::QueueFlagBits::eCompute)
    {
      queueIdx.compute = Helper::getQueueFamilyIndex (vk::QueueFlagBits::eCompute);

      // check if queue index already requested
      bool exists = false;
      for (const auto &createInfo : queueCreateInfos)
        {
          if (createInfo.queueFamilyIndex == queueIdx.compute)
            {
              exists = true;
              break;
            }
        }
      if (!exists)
        {
          vk::DeviceQueueCreateInfo computeQueueCreateInfo;
          computeQueueCreateInfo.queueCount = 1;
          computeQueueCreateInfo.queueFamilyIndex = queueIdx.compute;
          computeQueueCreateInfo.pQueuePriorities = &defaultQueuePriority;

          queueCreateInfos.push_back (computeQueueCreateInfo);
        }
    }

  std::vector<const char *> enabledExtensions;
  for (const auto &req : requestedLogicalDeviceExtensions)
    {
      enabledExtensions.push_back (req.c_str ());
    }
  vk::DeviceCreateInfo deviceCreateInfo;
  deviceCreateInfo.pNext = &physicalFeatures2;
  deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t> (queueCreateInfos.size ());
  deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data ();
  deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t> (enabledExtensions.size ());
  deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data ();

  // create logical device
  *logicalDevice = physicalDevice->createDevice (deviceCreateInfo);
  return;
}

void
Window::checkValidationSupport (void)
{
  std::vector<vk::LayerProperties> enumeratedInstLayers = vk::enumerateInstanceLayerProperties ();

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
  if (enumeratedInstExtensions.size () < 1 && !requestedInstanceExtensions.empty ())
    throw std::runtime_error ("No instance extensions found");

  std::string prelude = "The following instance extensions were not found...\n";
  std::string failed;
  for (const auto &reqInstExtensionName : requestedInstanceExtensions)
    {
      bool match = false;
      for (const auto &availableExtension : enumeratedInstExtensions)
        {
          if (strcmp (reqInstExtensionName, availableExtension.extensionName) == 0)
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
initialDebugProcessor (VkDebugUtilsMessageSeverityFlagBitsEXT p_MessageSeverity,
                       [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT p_MessageType,
                       const VkDebugUtilsMessengerCallbackDataEXT *p_CallbackData,
                       [[maybe_unused]] void *p_UserData)
{
  if (p_MessageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
      std::ostringstream oss;
      oss << "Vulkan Performance Validation => " << p_CallbackData->messageIdNumber << ", "
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
          << "Error: " << p_CallbackData->messageIdNumber << ", " << p_CallbackData->pMessageIdName
          << "\n"
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
generalDebugCallback (VkDebugUtilsMessageSeverityFlagBitsEXT p_MessageSeverity,
                      [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT p_MessageType,
                      const VkDebugUtilsMessengerCallbackDataEXT *p_CallbackData,
                      [[maybe_unused]] void *p_UserData)
{
  if (p_MessageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
      std::ostringstream oss;
      oss << "Vulkan Performance Validation => " << p_CallbackData->messageIdNumber << ", "
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
          << "Error: " << p_CallbackData->messageIdNumber << ", " << p_CallbackData->pMessageIdName
          << "\n"
          << p_CallbackData->pMessage << "\n\n";
      std::cout << oss.str ();
      throw std::runtime_error ("Vulkan error");
    }
  return false;
}