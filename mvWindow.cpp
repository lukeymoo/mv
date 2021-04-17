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
    swapChain.cleanup();
    destroyCommandBuffers();

    if (m_RenderPass)
    {
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
    }
    if (!frameBuffers.empty())
    {
        for (size_t i = 0; i < frameBuffers.size(); i++)
        {
            if (frameBuffers[i])
            {
                vkDestroyFramebuffer(m_Device, frameBuffers[i], nullptr);
            }
        }
    }

    if (depthStencil.image)
    {
        vkDestroyImage(m_Device, depthStencil.image, nullptr);
    }
    if (depthStencil.view)
    {
        vkDestroyImageView(m_Device, depthStencil.view, nullptr);
    }
    if (depthStencil.mem)
    {
        vkFreeMemory(m_Device, depthStencil.mem, nullptr);
    }

    if (m_PipelineCache)
    {
        vkDestroyPipelineCache(m_Device, m_PipelineCache, nullptr);
    }

    destroyCommandPool();
    vkDestroySemaphore(m_Device, semaphores.renderComplete, nullptr);
    vkDestroySemaphore(m_Device, semaphores.presentComplete, nullptr);
    for (auto &fence : waitFences)
    {
        vkDestroyFence(m_Device, fence, nullptr);
    }
    delete device;

    if (m_Instance)
    {
        vkDestroyInstance(m_Instance, nullptr);
    }
    xcb_disconnect(connection);
    return;
}

void mv::Window::prepare(void)
{
    // initialize vulkan
    if (!initVulkan())
    {
        throw std::runtime_error("Failed to initialize Vulkan");
    }

    swapChain.initSurface(connection, window);
    m_CommandPool = device->createCommandPool(swapChain.queueIndex);
    swapChain.create(&windowWidth, &windowHeight);
    createCommandBuffers();
    createSynchronizationPrimitives();
    setupDepthStencil();
    setupRenderPass();
    createPipelineCache();
    setupFramebuffer();
    return;
}

// void mv::Window::go(void)
// {
//     // initialize vulkan
//     if (!initVulkan())
//     {
//         throw std::runtime_error("Failed to initialize Vulkan");
//     }

//     swapChain.initSurface(connection, window);
//     m_CommandPool = device->createCommandPool(swapChain.queueIndex);
//     swapChain.create(&windowWidth, &windowHeight);
//     createCommandBuffers();
//     createSynchronizationPrimitives();
//     setupDepthStencil();
//     setupRenderPass();
//     createPipelineCache();
//     setupFramebuffer();

//     while ((event = xcb_wait_for_event(connection))) // replace this as it is a blocking call
//     {
//         switch (event->response_type & ~0x80)
//         {
//         default:
//             break;
//         }

//         free(event);
//     }
//     return;
// }

bool mv::Window::initVulkan(void)
{
    // Create instance
    if (createInstance() != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vulkan instance");
    }

    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr));
    if (deviceCount == 0)
    {
        throw std::runtime_error("No adapters found on system");
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
        throw std::runtime_error("Failed to create logical device");
    }

    m_Device = device->device;

    // get format
    depthFormat = device->getSupportedDepthFormat(m_PhysicalDevice);

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

void mv::Window::createCommandBuffers(void)
{
    cmdBuffers.resize(swapChain.imageCount);

    VkCommandBufferAllocateInfo allocInfo = mv::initializer::commandBufferAllocateInfo(m_CommandPool,
                                                                                       VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                                                       static_cast<uint32_t>(cmdBuffers.size()));
    if (vkAllocateCommandBuffers(m_Device, &allocInfo, cmdBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers");
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
            throw std::runtime_error("Failed to create wait fence");
        }
    }
    return;
}

void mv::Window::setupDepthStencil(void)
{
    // Create depth test image
    VkImageCreateInfo imageCI{};
    imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCI.imageType = VK_IMAGE_TYPE_2D;
    imageCI.format = depthFormat;
    imageCI.extent = {windowWidth, windowHeight, 1};
    imageCI.mipLevels = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (vkCreateImage(m_Device, &imageCI, nullptr, &depthStencil.image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth stencil image");
    }

    // Allocate memory for image
    VkMemoryRequirements memReq = {};
    vkGetImageMemoryRequirements(m_Device, depthStencil.image, &memReq);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = device->getMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_Device, &allocInfo, nullptr, &depthStencil.mem) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory for depth stencil image");
    }
    if (vkBindImageMemory(m_Device, depthStencil.image, depthStencil.mem, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to bind depth stencil image and memory");
    }

    // Create view
    VkImageViewCreateInfo ivInfo = {};
    ivInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivInfo.image = depthStencil.image;
    ivInfo.format = depthFormat;
    ivInfo.subresourceRange.baseMipLevel = 0;
    ivInfo.subresourceRange.levelCount = 1;
    ivInfo.subresourceRange.baseArrayLayer = 0;
    ivInfo.subresourceRange.layerCount = 1;
    ivInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // for stencil + depth formats
    if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        ivInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (vkCreateImageView(m_Device, &ivInfo, nullptr, &depthStencil.view) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth stencil image view");
    }
    return;
}

void mv::Window::setupRenderPass(void)
{
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = swapChain.colorFormat;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format = depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef = {};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef = {};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDesc = {};
    subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDesc.colorAttachmentCount = 1;
    subpassDesc.pColorAttachments = &colorRef;
    subpassDesc.pDepthStencilAttachment = &depthRef;
    subpassDesc.inputAttachmentCount = 0;
    subpassDesc.pInputAttachments = nullptr;
    subpassDesc.preserveAttachmentCount = 0;
    subpassDesc.pPreserveAttachments = nullptr;
    subpassDesc.pResolveAttachments = nullptr;

    std::array<VkSubpassDependency, 2> dependencies = {};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDesc;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render pass");
    }
    return;
}

void mv::Window::createPipelineCache(void)
{
    VkPipelineCacheCreateInfo pcinfo = {};
    pcinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(m_Device, &pcinfo, nullptr, &m_PipelineCache) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline cache");
    }
    return;
}

void mv::Window::setupFramebuffer(void)
{
    VkImageView attachments[2];

    attachments[1] = depthStencil.view;

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_RenderPass;
    framebufferInfo.attachmentCount = 2;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = windowWidth;
    framebufferInfo.height = windowHeight;
    framebufferInfo.layers = 1;

    // Framebuffer per swap image
    frameBuffers.resize(swapChain.imageCount);
    for (size_t i = 0; i < frameBuffers.size(); i++)
    {
        attachments[0] = swapChain.buffers[i].view;
        if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &frameBuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create frame buffer");
        }
    }
    return;
}

void mv::Window::checkValidationSupport(void)
{
    uint32_t layerCount = 0;
    if (vkEnumerateInstanceLayerProperties(&layerCount, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query supported instance layer count");
    }

    if (layerCount == 0 && !requestedValidationLayers.empty())
    {
        throw std::runtime_error("No supported validation layers found");
    }

    std::vector<VkLayerProperties> availableLayers(layerCount);
    if (vkEnumerateInstanceLayerProperties(&layerCount,
                                           availableLayers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query supported instance layer list");
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
        throw std::runtime_error(prelude + failed);
    }
    else
    {
        std::cout << "[+] System supports all requested validation layers" << std::endl;
    }
    return;
}

void mv::Window::destroyCommandBuffers(void)
{
    if (!cmdBuffers.empty())
    {
        vkFreeCommandBuffers(m_Device, m_CommandPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
    }
    return;
}

void mv::Window::destroyCommandPool(void)
{
    if (m_CommandPool != nullptr)
    {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
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
        throw std::runtime_error("Failed to query instance supported extension count");
    }

    if (instanceExtensionCount == 0 && !requestedInstanceExtensions.empty())
    {
        throw std::runtime_error("No instance level extensions supported by device");
    }

    std::vector<VkExtensionProperties> availableInstanceExtensions(instanceExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr,
                                               &instanceExtensionCount,
                                               availableInstanceExtensions.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query instance supported extensions list");
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
        throw std::runtime_error(prelude + failed);
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

std::vector<char> mv::Window::readFile(std::string filename)
{
    size_t fileSize;
    std::ifstream file;
    std::vector<char> buffer;

    // check if file exists
    try
    {
        std::filesystem::exists(filename);
        file.open(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << "Failed to open file " << filename;
            throw std::runtime_error(oss.str());
        }

        // prepare buffer to hold shader bytecode
        fileSize = (size_t)file.tellg();
        buffer.resize(fileSize);

        // go back to beginning of file and read in
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
    }
    catch (std::filesystem::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Filesystem Exception : " << e.what() << std::endl;
        throw std::runtime_error(oss.str());
    }
    catch (std::exception &e)
    {
        std::ostringstream oss;
        oss << "Standard Exception : " << e.what();
        throw std::runtime_error(oss.str());
    }
    catch (...)
    {
        throw std::runtime_error("Unhandled exception while loading a file");
    }

    if (buffer.empty())
    {
        throw std::runtime_error("File reading operation returned empty buffer");
    }

    return buffer;
}

VkShaderModule mv::Window::createShaderModule(const std::vector<char> &code)
{
    VkShaderModule module;

    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.pNext = nullptr;
    moduleInfo.flags = 0;
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    if (vkCreateShaderModule(m_Device, &moduleInfo, nullptr, &module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    return module;
}