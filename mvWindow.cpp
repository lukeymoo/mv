#include "mvWindow.h"

mv::Window::Window(int w, int h, std::string title)
{
    this->windowWidth = w;
    this->windowHeight = h;
    this->title = title;

    glfwInit();

    uint32_t count = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&count);
    if (!extensions)
        throw std::runtime_error("Failed to get required instance extensions");

    // Get our own list of requested extensions
    for (const auto &req : requestedInstanceExtensions)
    {
        instanceExtensions.push_back(req);
    }

    // Get GLFW requested extensions
    std::vector<std::string> glfw_requested;
    for (uint32_t i = 0; i < count; i++)
    {
        glfw_requested.push_back(extensions[i]);
    }

    // temp container for missing extensions
    std::vector<std::string> tmp;

    // iterate glfw requested
    for (const auto &glfw_req : glfw_requested)
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
            instanceExtensions.push_back(glfw_req);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(windowWidth, windowHeight, title.c_str(), nullptr, nullptr);
    if (!window)
        throw std::runtime_error("Failed to create window");

    // glfwSetWindowPos(window, 0, 0);

    // set keyboard callback
    glfwSetKeyCallback(window, mv::keyCallback);

    // set mouse motion callback
    glfwSetCursorPosCallback(window, mv::mouseMotionCallback);

    // set mouse button callback
    glfwSetMouseButtonCallback(window, mv::mouseButtonCallback);

    auto is_raw_supp = glfwRawMouseMotionSupported();

    if (!is_raw_supp)
        throw std::runtime_error("Raw mouse motion not supported");

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, true);

    // configure callback
    glfwSetWindowUserPointer(window, this);

    // Mouse and keyboard methods now belong to namespaces
    // mouse/keyboard respectively
    // kbd = std::make_unique<mv::keyboard>(window);
    // mouse = std::make_unique<mv::mouse>(window);

    // initialize smart pointers
    swapchain = std::make_unique<mv::Swap>();
    commandBuffers = std::make_unique<std::vector<vk::CommandBuffer>>();
    coreFramebuffers = std::make_unique<std::vector<vk::Framebuffer>>();
    guiFramebuffers = std::make_unique<std::vector<vk::Framebuffer>>();
    inFlightFences = std::make_unique<std::vector<vk::Fence>>();
    waitFences = std::make_unique<std::vector<vk::Fence>>();
    semaphores = std::make_unique<struct SemaphoresStruct>();
    depthStencil = std::make_unique<struct DepthStencilStruct>();
    return;
}

mv::Window::~Window()
{
    if (!mvDevice)
        return;
    // ensure gpu not using any resources
    if (mvDevice->logicalDevice)
    {
        mvDevice->logicalDevice->waitIdle();
    }

    // swapchain related resource cleanup
    swapchain->cleanup(*instance, *mvDevice);

    // cleanup command buffers
    if (commandBuffers)
    {
        if (!commandBuffers->empty())
        {
            mvDevice->logicalDevice->freeCommandBuffers(*commandPool, *commandBuffers);
        }
        commandBuffers.reset();
    }

    // cleanup sync objects
    if (inFlightFences)
    {
        for (auto &fence : *inFlightFences)
        {
            if (fence)
            {
                mvDevice->logicalDevice->destroyFence(fence, nullptr);
            }
        }
        inFlightFences.reset();
    }
    // ''
    if (semaphores)
    {
        if (semaphores->presentComplete)
            mvDevice->logicalDevice->destroySemaphore(semaphores->presentComplete, nullptr);
        if (semaphores->renderComplete)
            mvDevice->logicalDevice->destroySemaphore(semaphores->renderComplete, nullptr);
        semaphores.reset();
    }

    if (renderPass)
    {
        mvDevice->logicalDevice->destroyRenderPass(*renderPass, nullptr);
        renderPass.reset();
    }

    // core engine render framebuffers
    if (coreFramebuffers)
    {
        if (!coreFramebuffers->empty())
        {
            for (auto &buffer : *coreFramebuffers)
            {
                if (buffer)
                {
                    mvDevice->logicalDevice->destroyFramebuffer(buffer, nullptr);
                }
            }
            coreFramebuffers.reset();
        }
    }

    // ImGui framebuffers
    if (guiFramebuffers)
    {
        if (!guiFramebuffers->empty())
        {
            for (auto &buffer : *guiFramebuffers)
            {
                if (buffer)
                {
                    mvDevice->logicalDevice->destroyFramebuffer(buffer, nullptr);
                }
            }
            guiFramebuffers.reset();
        }
    }

    if (depthStencil)
    {
        if (depthStencil->image)
        {
            mvDevice->logicalDevice->destroyImage(depthStencil->image, nullptr);
        }
        if (depthStencil->view)
        {
            mvDevice->logicalDevice->destroyImageView(depthStencil->view, nullptr);
        }
        if (depthStencil->mem)
        {
            mvDevice->logicalDevice->freeMemory(depthStencil->mem, nullptr);
        }
        depthStencil.reset();
    }

    if (commandPool)
    {
        mvDevice->logicalDevice->destroyCommandPool(*commandPool);
        commandPool.reset();
    }

    // custom logical device interface/container cleanup
    if (mvDevice)
    {
        mvDevice.reset();
    }

    if (instance)
    {
        instance->destroy();
        instance.reset();
    }

    if (window)
        glfwDestroyWindow(window);
    glfwTerminate();
    return;
}

void mv::Window::prepare(void)
{
    // creates...
    // physical device
    // logical device
    // swapchain surface
    // passes... vulkan handles to swapchain handler
    initVulkan();

    swapchain->init(window, *instance, *physicalDevice);
    commandPool = std::make_unique<vk::CommandPool>(mvDevice->createCommandPool(mvDevice->queueIdx.graphics));

    swapchain->create(*physicalDevice, *mvDevice, windowWidth, windowHeight);

    createCommandBuffers();

    createSynchronizationPrimitives();

    setupDepthStencil();

    setupRenderPass();

    // create_pipeline_cache();

    setupFramebuffer();

    // load device extension functions
    pfn_vkCmdSetPrimitiveTopology = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(
        vkGetInstanceProcAddr(*instance, "vkCmdSetPrimitiveTopologyEXT"));
    if (!pfn_vkCmdSetPrimitiveTopology)
        throw std::runtime_error("Failed to load extended dynamic state extensions");
    return;
}

void mv::Window::initVulkan(void)
{
    // creates vulkan instance with specified instance extensions/layers
    createInstance();

    std::vector<vk::PhysicalDevice> physicalDevices = instance->enumeratePhysicalDevices();

    if (physicalDevices.size() < 1)
        throw std::runtime_error("No physical devices found");

    // Select device
    physicalDevice = std::make_unique<vk::PhysicalDevice>(std::move(physicalDevices.at(0)));
    // resize devices container
    physicalDevices.erase(physicalDevices.begin());

    // get device info
    physicalProperties = physicalDevice->getProperties();
    physicalFeatures = physicalDevice->getFeatures();
    physicalMemoryProperties = physicalDevice->getMemoryProperties();

    std::vector<std::string> tmp;
    for (const auto &extensionName : requestedDeviceExtensions)
    {
        std::cout << "\t[-] Requesting device extension => " << extensionName << "\n";
        tmp.push_back(extensionName);
    }

    // create logical device handler mv::Device
    mvDevice = std::make_unique<mv::Device>(*physicalDevice, tmp);

    // create logical device & graphics queue
    mvDevice->createLogicalDevice(*physicalDevice);

    // get format
    depthFormat = mvDevice->getSupportedDepthFormat(*physicalDevice);

    // no longer pass references to swapchain, pass reference on per function basis now
    // swapchain->map(std::weak_ptr<vk::Instance>(instance),
    //                std::weak_ptr<vk::PhysicalDevice>(physical_device),
    //                std::weak_ptr<mv::Device>(mv_device));

    // Create synchronization objects
    vk::SemaphoreCreateInfo semaphoreInfo;
    semaphores->presentComplete = mvDevice->logicalDevice->createSemaphore(semaphoreInfo);
    semaphores->renderComplete = mvDevice->logicalDevice->createSemaphore(semaphoreInfo);

    return;
}

void mv::Window::createInstance(void)
{
    // Ensure we have validation layers
#ifndef NDEBUG
    checkValidationSupport();
#endif

    // Ensure we have all requested instance extensions
    checkInstanceExt();

    vk::ApplicationInfo appInfo;
    appInfo.pApplicationName = "Bloody Day";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Moogin";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_MAKE_VERSION(1, 2, 0);

/* If debugging enabled */
#ifndef NDEBUG
    vk::DebugUtilsMessengerCreateInfoEXT debuggerSettings;
    debuggerSettings.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                       vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debuggerSettings.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                   vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                   vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    debuggerSettings.pfnUserCallback = debug_message_processor;
    debuggerSettings.pUserData = nullptr;

    // convert string request to const char*
    std::vector<const char *> req_layers;
    for (auto &layerName : requestedValidationLayers)
    {
        std::cout << "\t[-] Requesting layer => " << layerName << "\n";
        req_layers.push_back(layerName);
    }
    std::vector<const char *> req_inst_ext;
    for (auto &ext : instanceExtensions)
    {
        std::cout << "\t[-] Requesting instance extension => " << ext << "\n";
        req_inst_ext.push_back(ext.c_str());
    }

    vk::InstanceCreateInfo createInfo;
    createInfo.pNext = &debuggerSettings;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledLayerCount = static_cast<uint32_t>(req_layers.size());
    createInfo.ppEnabledLayerNames = req_layers.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(req_inst_ext.size());
    createInfo.ppEnabledExtensionNames = req_inst_ext.data();
#endif
#ifdef NDEBUG /* Debugging disabled */
    std::vector<const char *> req_inst_ext;
    for (auto &ext : instanceExtensions)
    {
        std::cout << "\t[-] Requesting instance extension => " << ext << "\n";
        req_inst_ext.push_back(ext.c_str());
    }

    vk::InstanceCreateInfo createInfo;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(req_inst_ext.size());
    createInfo.ppEnabledExtensionNames = req_inst_ext.data();
#endif

    instance = std::make_unique<vk::Instance>(vk::createInstance(createInfo));
    // double check instance(prob a triple check at this point)
    if (!*instance)
        throw std::runtime_error("Failed to create vulkan instance");

    return;
}

void mv::Window::createCommandBuffers(void)
{
    commandBuffers->resize(swapchain->images->size());

    vk::CommandBufferAllocateInfo allocInfo;
    allocInfo.commandPool = *commandPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers->size());

    *commandBuffers = mvDevice->logicalDevice->allocateCommandBuffers(allocInfo);

    if (commandBuffers->size() < 1)
        throw std::runtime_error("Failed to allocate command buffers");
    return;
}

void mv::Window::createSynchronizationPrimitives(void)
{
    vk::FenceCreateInfo fenceInfo;
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;

    waitFences->resize(swapchain->images->size(), nullptr);
    inFlightFences->resize(MAX_IN_FLIGHT);

    for (auto &fence : *inFlightFences)
    {
        fence = mvDevice->logicalDevice->createFence(fenceInfo);
    }
    return;
}

void mv::Window::setupDepthStencil(void)
{
    vk::ImageCreateInfo imageInfo;
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = depthFormat;
    imageInfo.extent = vk::Extent3D{windowWidth, windowHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

    // create depth stencil testing image
    depthStencil->image = mvDevice->logicalDevice->createImage(imageInfo);

    // Allocate memory for image
    vk::MemoryRequirements memReq;
    memReq = mvDevice->logicalDevice->getImageMemoryRequirements(depthStencil->image);

    vk::MemoryAllocateInfo allocInfo;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        mvDevice->getMemoryType(memReq.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    // allocate depth stencil image memory
    depthStencil->mem = mvDevice->logicalDevice->allocateMemory(allocInfo);

    // bind image and memory
    mvDevice->logicalDevice->bindImageMemory(depthStencil->image, depthStencil->mem, 0);

    // Create view into depth stencil testing image
    vk::ImageViewCreateInfo viewInfo;
    viewInfo.viewType = vk::ImageViewType::e2D;
    viewInfo.image = depthStencil->image;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    // if physical device supports high enough format add stenciling
    if (depthFormat >= vk::Format::eD16UnormS8Uint)
    {
        viewInfo.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }

    depthStencil->view = mvDevice->logicalDevice->createImageView(viewInfo);
    return;
}

void mv::Window::setupRenderPass(void)
{
    std::array<vk::AttachmentDescription, 3> attachments;
    // Color attachment
    attachments[0].format = swapchain->color_format;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

    // Depth attachment
    attachments[1].format = swapchain->depthFormat;
    attachments[1].samples = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].initialLayout = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    // ImGui attachment
    attachments[2].format = swapchain->colorFormat;
    attachments[2].samples = vk::SampleCountFlagBits::e1;
    attachments[2].loadOp = vk::AttachmentLoadOp::eLoad;
    attachments[2].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[2].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachments[2].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference colorRef;
    colorRef.attachment = 0;
    colorRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depthRef;
    depthRef.attachment = 1;
    depthRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference guiRef;
    guiRef.attachment = 2;
    guiRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

    // color attachment and depth testing subpass
    vk::SubpassDescription corePass;
    corePass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    corePass.colorAttachmentCount = 1;
    corePass.pColorAttachments = &colorRef;
    corePass.pDepthStencilAttachment = &depthRef;

    // imgui rendering
    vk::SubpassDescription guiPass;
    guiPass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    guiPass.colorAttachmentCount = 1;
    guiPass.pColorAttachments = &guiRef;

    std::vector<vk::SubpassDescription> subpasses = {
        corePass,
        guiPass,
    };

    std::array<vk::SubpassDependency, 2> dependencies;
    // Color + depth subpass
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[0].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    // ImGui subpass
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].srcAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[1].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::RenderPassCreateInfo renderPassInfo;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    renderPass = std::make_unique<vk::RenderPass>(mvDevice->logicalDevice->createRenderPass(renderPassInfo));
    return;
}

// TODO
// Re add call to this method
// void mv::MWindow::create_pipeline_cache(void)
// {
//     vk::PipelineCacheCreateInfo pcinfo;
//     pipeline_cache =
//     std::make_shared<vk::PipelineCache>(mv_device->logical_device->createPipelineCache(pcinfo));
//     return;
// }

void mv::Window::setupFramebuffer(void)
{
    // Attachments for core engine rendering
    std::array<vk::ImageView, 2> attachments;
    // Attachment for ImGui rendering
    vk::ImageView imguiAttachment;

    // each frame buffer uses same depth image
    attachments[1] = depthStencil->view;

    // core engine render will use color attachment buffer & depth buffer
    vk::FramebufferCreateInfo coreFrameInfo;
    coreFrameInfo.renderPass = *renderPass;
    coreFrameInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    coreFrameInfo.pAttachments = attachments.data();
    coreFrameInfo.width = swapchain->swap_extent.width;
    coreFrameInfo.height = swapchain->swap_extent.height;
    coreFrameInfo.layers = 1;

    // ImGui must only have 1 attachment
    vk::FramebufferCreateInfo guiFrameInfo;
    guiFrameInfo.renderPass = *renderPass;
    guiFrameInfo.attachmentCount = 1;
    guiFrameInfo.pAttachments = &imguiAttachment;
    guiFrameInfo.width = swapchain->swap_extent.width;
    guiFrameInfo.height = swapchain->swap_extent.height;
    guiFrameInfo.layers = 1;

    // Framebuffer per swap image
    coreFramebuffers->resize(static_cast<uint32_t>(swapchain->images->size()));
    guiFramebuffers->resize(static_cast<uint32_t>(swapchain->images->size()));
    for (size_t i = 0; i < coreFramebuffers->size(); i++)
    {
        // Assign each swapchain image to a frame buffer
        attachments[0] = swapchain->buffers->at(i).view;
        imguiAttachment = swapchain->buffers->at(i).view;

        coreFramebuffers->at(i) = mvDevice->logicalDevice->createFramebuffer(coreFrameInfo);
        guiFramebuffers->at(i) = mvDevice->logicalDevice->createFramebuffer(guiFrameInfo);
    }
    return;
}

void mv::Window::checkValidationSupport(void)
{
    std::vector<vk::LayerProperties> enumeratedInstLayers = vk::enumerateInstanceLayerProperties();

    if (enumeratedInstLayers.size() == 0 && !requestedValidationLayers.empty())
        throw std::runtime_error("No supported validation layers found");

    // look for missing requested layers
    std::string prelude = "The following instance layers were not found...\n";
    std::string failed;
    for (const auto &reqLayerName : requestedValidationLayers)
    {
        bool match = false;
        for (const auto &layer : enumeratedInstLayers)
        {
            if (strcmp(reqLayerName, layer.layerName) == 0)
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

void mv::Window::checkInstanceExt(void)
{
    std::vector<vk::ExtensionProperties> enumeratedInstExtensions = vk::enumerateInstanceExtensionProperties();

    // use f_req vector for instance extensions
    if (enumeratedInstExtensions.size() < 1 && !requestedInstanceExtensions.empty())
        throw std::runtime_error("No instance extensions found");

    std::string prelude = "The following instance extensions were not found...\n";
    std::string failed;
    for (const auto &reqInstExtensionName : requestedInstanceExtensions)
    {
        bool match = false;
        for (const auto &availableExtension : enumeratedInstExtensions)
        {
            if (strcmp(reqInstExtensionName, availableExtension.extensionName) == 0)
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

    if (!failed.empty())
    {
        throw std::runtime_error(prelude + failed);
    }

    return;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_message_processor(VkDebugUtilsMessageSeverityFlagBitsEXT p_MessageSeverity,
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
    }
    if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Warning: " << p_CallbackData->messageIdNumber << ", " << p_CallbackData->pMessageIdName << "\n"
            << p_CallbackData->pMessage << "\n\n";
        std::cout << oss.str();
    }
    // else if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    // {
    //     // Disabled by the impossible statement
    //     std::ostringstream oss;
    //     oss << std::endl
    //         << "Verbose message : " << p_CallbackData->messageIdNumber << ", " << p_CallbackData->pMessageIdName
    //         << std::endl
    //         << p_CallbackData->pMessage << "\n\n";
    //     std::cout << oss.str();
    // }
    else if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Error: " << p_CallbackData->messageIdNumber << ", " << p_CallbackData->pMessageIdName << "\n"
            << p_CallbackData->pMessage << "\n\n";
        std::cout << oss.str();
    }
    // else if (p_MessageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    // {
    //     std::ostringstream oss;
    //     oss << std::endl
    //         << "Info: " << p_CallbackData->messageIdNumber << ", " << p_CallbackData->pMessageIdName << "\n"
    //         << p_CallbackData->pMessage << "\n\n";
    //     std::cout << oss.str();
    // }
    return false;
}

std::vector<char> mv::Window::readFile(std::string p_Filename)
{
    size_t fileSize;
    std::ifstream file;
    std::vector<char> buffer;

    // check if file exists
    try
    {
        std::filesystem::exists(p_Filename);
        file.open(p_Filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << "Failed to open file " << p_Filename;
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
        throw std::runtime_error("File reading operation returned empty buffer :: shaders?");
    }

    return buffer;
}

vk::ShaderModule mv::Window::createShaderModule(const std::vector<char> &p_ShaderCharBuffer)
{
    vk::ShaderModuleCreateInfo moduleInfo;
    moduleInfo.codeSize = p_ShaderCharBuffer.size();
    moduleInfo.pCode = reinterpret_cast<const uint32_t *>(p_ShaderCharBuffer.data());

    vk::ShaderModule module = mvDevice->logicalDevice->createShaderModule(moduleInfo);

    return module;
}
