#include "mvWindow.h"

mv::Window::Exception::Exception(int l, std::string f, std::string description)
    : BException(l, f, description)
{
    type = "Window Handler Exception";
    errorDescription = description;
    return;
}

mv::Window::Exception::~Exception(void)
{
    return;
}

/* Create window and initialize Vulkan */
mv::Window::Window(int w, int h, const char *title)
{
    uint32_t mask = 0;
    uint32_t values[2];

    // Connect to x server
    connection = xcb_connect(NULL, NULL);

    // get first screen
    const xcb_setup_t *setup = xcb_get_setup(connection);
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
    xcb_screen_t *screen = iter.data;

    mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    values[0] = screen->white_pixel;
    values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
                XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
                XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE;

    // create window
    window = xcb_generate_id(connection);
    xcb_create_window(connection,
                      XCB_COPY_FROM_PARENT,
                      window,
                      screen->root,
                      300, 300,
                      WINDOW_WIDTH,
                      WINDOW_HEIGHT,
                      1,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,
                      mask, values);

    xcb_change_property(connection,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        strlen(title),
                        title);

    // map window
    xcb_map_window(connection, window);

    xcb_flush(connection);

    return;
}

mv::Window::~Window(void)
{
    // add here
    delete device;

    if (m_Instance)
    {
        vkDestroyInstance(m_Instance, nullptr);
    }
    xcb_disconnect(connection);
    return;
}

void mv::Window::go(void)
{
    // initialize vulkan
    if (!initVulkan())
    {
        W_EXCEPT("Failed to initialize Vulkan");
    }

    swapChain.initSurface(connection, window);
    m_CommandPool = device->createCommandPool(swapChain.queueIndex);
    swapChain.create(&windowWidth, &windowHeight);
    createCommandBuffers();
    createSynchronizationPrimitives();
    setupDepthStencil();

    while ((event = xcb_wait_for_event(connection))) // replace this as it is a blocking call
    {
        switch (event->response_type & ~0x80)
        {
        default:
            break;
        }

        free(event);
    }
    return;
}

bool mv::Window::initVulkan(void)
{
    // Create instance
    if (createInstance() != VK_SUCCESS)
    {
        W_EXCEPT("Failed to create vulkan instance");
    }

    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));
    if (deviceCount == 0)
    {
        W_EXCEPT("No adapters found on system");
    }
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, physicalDevices.data()));

    // Select device
    m_PhysicalDevice = physicalDevices[0];

    // Get device info
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
    vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &features);
    vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memoryProperties);

    device = new mv::Device(m_PhysicalDevice);
    if (device->createLogicalDevice(features, requestedDeviceExtensions) != VK_SUCCESS)
    {
        W_EXCEPT("Failed to create logical device");
    }

    m_Device = device->m_Device;

    // get format
    format = device->getSupportedDepthFormat(m_PhysicalDevice);

    // get graphics queue
    vkGetDeviceQueue(m_Device, device->queueFamilyIndices.graphics, 0, &m_GraphicsQueue);

    swapChain.connect(m_Instance, m_PhysicalDevice, m_Device);

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphoreInfo = mv::initializer::semaphoreCreateInfo();
    VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &semaphores.presentComplete));
    VK_CHECK(vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &semaphores.renderComplete));

    // submit info obj
    submitInfo = mv::initializer::submitInfo();
    submitInfo.pWaitDstStageMask = &stageFlags;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &semaphores.presentComplete;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &semaphores.renderComplete;

    return true;
}

VkResult mv::Window::createInstance(void)
{
    // Ensure we have validation layers
#ifndef NDEBUG
    checkValidationSupport();
#endif

    // Ensure we have all requested instance extensions
    checkInstanceExt();

    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "Bloody Day";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Moogin";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);

/* If debugging enabled */
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debuggerSettings{};
    debuggerSettings.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debuggerSettings.pNext = nullptr;
    debuggerSettings.flags = 0;
    debuggerSettings.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debuggerSettings.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debuggerSettings.pfnUserCallback = debugMessageProcessor;
    debuggerSettings.pUserData = nullptr;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = &debuggerSettings;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(requestedValidationLayers.size());
    createInfo.ppEnabledLayerNames = requestedValidationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requestedInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = requestedInstanceExtensions.data();
#endif
#ifdef NDEBUG /* Debugging disabled */
    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(requestedValidationLayers.size());
    createInfo.ppEnabledLayerNames = requestedValidationLayers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(requestedInstanceExtensions.size());
    createInfo.ppEnabledExtensionNames = requestedInstanceExtensions.data();
#endif

    return vkCreateInstance(&createInfo, nullptr, &m_Instance);
}

void mv::Window::setupDepthStencil(void)
{
    return;
}

void mv::Window::checkValidationSupport(void)
{
    uint32_t layerCount = 0;
    if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS)
    {
        W_EXCEPT("Failed to query supported instance layer count");
    }

    if (layerCount == 0 && !requestedValidationLayers.empty())
    {
        W_EXCEPT("No supported validation layers found");
    }

    std::vector<VkLayerProperties> availableLayers(layerCount);
    if (vkEnumerateInstanceLayerProperties(&layerCount,
                                           availableLayers.data()) != VK_SUCCESS)
    {
        W_EXCEPT("Failed to query supported instance layer list");
    }

    std::string prelude = "The following instance layers were not found...\n";
    std::string failed;
    for (const auto &requestedLayer : requestedValidationLayers)
    {
        bool match = false;
        for (const auto &availableLayer : availableLayers)
        {
            if (strcmp(requestedLayer, availableLayer.layerName) == 0)
            {
                match = true;
                break;
            }
        }
        if (!match)
        {
            failed += requestedLayer;
            failed += "\n";
        }
    }

    if (!failed.empty())
    {
        W_EXCEPT(prelude + failed);
    }
    else
    {
        std::cout << "[+] System supports all requested validation layers" << std::endl;
    }
    return;
}

void mv::Window::createCommandBuffers(void)
{
    cmdBuffers.resize(swapChain.imageCount);

    VkCommandBufferAllocateInfo allocInfo = mv::initializer::commandBufferAllocateInfo(m_CommandPool,
                                                                                       VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                                                       static_cast<uint32_t>(cmdBuffers.size()));
    if (vkAllocateCommandBuffers(m_Device, &allocInfo, cmdBuffers.data()) != VK_SUCCESS)
    {
        std::runtime_error("Failed to allocate command buffers");
    }
    return;
}

void mv::Window::createSynchronizationPrimitives(void)
{
    VkFenceCreateInfo fenceInfo = mv::initializer::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    waitFences.resize(cmdBuffers.size());
    for (auto &fence : waitFences)
    {
        if (vkCreateFence(m_Device, &fenceInfo, nullptr, &fence) != VK_SUCCESS)
        {
            std::runtime_error("Failed to create wait fence");
        }
    }
    return;
}

void mv::Window::checkInstanceExt(void)
{
    uint32_t instanceExtensionCount = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr,
                                               &instanceExtensionCount,
                                               nullptr) != VK_SUCCESS)
    {
        W_EXCEPT("Failed to query instance supported extension count");
    }

    if (instanceExtensionCount == 0 && !requestedInstanceExtensions.empty())
    {
        W_EXCEPT("No instance level extensions supported by device");
    }

    std::vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr,
                                               &instanceExtensionCount,
                                               availableInstanceExtensions.data()) != VK_SUCCESS)
    {
        W_EXCEPT("Failed to query instance supported extensions list");
    }

    std::string prelude = "The following instance extensions were not found...\n";
    std::string failed;
    for (const auto &requestedExtension : requestedInstanceExtensions)
    {
        bool match = false;
        for (const auto &availableExtension : availableInstanceExtensions)
        {
            // check if match
            if (strcmp(requestedExtension, availableExtension.extensionName) == 0)
            {
                match = true;
                break;
            }
        }

        if (!match)
        {
            failed += requestedExtension;
            failed += "\n";
        }
    }

    if (!failed.empty())
    {
        W_EXCEPT(prelude + failed);
    }

    return;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageProcessor(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                     VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                     const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                                     void *user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Warning: " << callback_data->messageIdNumber
            << ", " << callback_data->pMessageIdName << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
        std::cout << oss.str();
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT && 1 == 2)
    {
        // Disabled by the impossible statement
        std::ostringstream oss;
        oss << std::endl
            << "Verbose message : " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName
            << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
        std::cout << oss.str();
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Error: " << callback_data->messageIdNumber
            << ", " << callback_data->pMessageIdName << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
        std::cout << oss.str();
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Info: " << callback_data->messageIdNumber
            << ", " << callback_data->pMessageIdName << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
    }
    return VK_FALSE;
}