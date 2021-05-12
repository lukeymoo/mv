#include "mvWindow.h"

mv::MWindow::MWindow(int w, int h, std::string title)
{
    this->window_width = w;
    this->window_height = h;
    this->title = title;

    glfwInit();

    uint32_t count = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&count);
    if (!extensions)
        throw std::runtime_error("Failed to get required instance extensions");

    std::vector<std::string> req_ext;
    for (const auto &req : req_instance_extensions)
    {
        req_ext.push_back(req);
    }
    std::vector<std::string> glfw_requested;
    for (uint32_t i = 0; i < count; i++)
    {
        glfw_requested.push_back(extensions[i]);
    }

    // iterate glfw requested
    for (const auto &glfw_req : glfw_requested)
    {
        bool found = false;

        // iterate already requested list
        for (const auto &req : req_ext)
        {
            if (glfw_req == req)
            {
                found = true;
            }
        }

        // if not found, add to final list
        if (!found)
            f_req.push_back(glfw_req);
    }
    // concat existing requests with glfw requests into final list
    f_req.insert(f_req.end(), req_ext.begin(), req_ext.end());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(window_width, window_height, title.c_str(), nullptr, nullptr);
    if (!window)
        throw std::runtime_error("Failed to create window");

    // move window to right
    glfwSetWindowPos(window, 0, 0);

    // set keyboard callback
    glfwSetKeyCallback(window, mv::key_callback);

    // set mouse motion callback
    glfwSetCursorPosCallback(window, mv::mouse_motion_callback);

    // set mouse button callback
    glfwSetMouseButtonCallback(window, mv::mouse_button_callback);

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
    command_buffers = std::make_unique<std::vector<vk::CommandBuffer>>();
    frame_buffers = std::make_unique<std::vector<vk::Framebuffer>>();
    in_flight_fences = std::make_unique<std::vector<vk::Fence>>();
    wait_fences = std::make_unique<std::vector<vk::Fence>>();
    semaphores = std::make_unique<struct semaphores_struct>();
    depth_stencil = std::make_unique<struct depth_stencil_struct>();
    return;
}

mv::MWindow::~MWindow()
{
    if (!mv_device)
        return;
    // ensure gpu not using any resources
    if (mv_device->logical_device)
    {
        mv_device->logical_device->waitIdle();
    }

    // swapchain related resource cleanup
    swapchain->cleanup(*instance, *mv_device);

    // cleanup command buffers
    if (command_buffers)
    {
        if (!command_buffers->empty())
        {
            mv_device->logical_device->freeCommandBuffers(*command_pool, *command_buffers);
        }
        command_buffers.reset();
    }

    // cleanup sync objects
    if (in_flight_fences)
    {
        for (auto &fence : *in_flight_fences)
        {
            if (fence)
            {
                mv_device->logical_device->destroyFence(fence, nullptr);
            }
        }
        in_flight_fences.reset();
    }
    // ''
    if (semaphores)
    {
        if (semaphores->present_complete)
            mv_device->logical_device->destroySemaphore(semaphores->present_complete, nullptr);
        if (semaphores->render_complete)
            mv_device->logical_device->destroySemaphore(semaphores->render_complete, nullptr);
        semaphores.reset();
    }

    if (render_pass)
    {
        mv_device->logical_device->destroyRenderPass(*render_pass, nullptr);
        render_pass.reset();
    }

    if (frame_buffers)
    {
        if (!frame_buffers->empty())
        {
            for (auto &buffer : *frame_buffers)
            {
                if (buffer)
                {
                    mv_device->logical_device->destroyFramebuffer(buffer, nullptr);
                }
            }
            frame_buffers.reset();
        }
    }

    if (depth_stencil)
    {
        if (depth_stencil->image)
        {
            mv_device->logical_device->destroyImage(depth_stencil->image, nullptr);
        }
        if (depth_stencil->view)
        {
            mv_device->logical_device->destroyImageView(depth_stencil->view, nullptr);
        }
        if (depth_stencil->mem)
        {
            mv_device->logical_device->freeMemory(depth_stencil->mem, nullptr);
        }
        depth_stencil.reset();
    }

    if (command_pool)
    {
        mv_device->logical_device->destroyCommandPool(*command_pool);
        command_pool.reset();
    }

    // custom logical device interface/container cleanup
    if (mv_device)
    {
        mv_device.reset();
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

void mv::MWindow::prepare(void)
{
    // creates...
    // physical device
    // logical device
    // swapchain surface
    // passes... vulkan handles to swapchain handler
    init_vulkan();

    swapchain->init(window, *instance, *physical_device);
    command_pool = std::make_unique<vk::CommandPool>(mv_device->create_command_pool(swapchain->graphics_index));

    swapchain->create(*physical_device, *mv_device, window_width, window_height);

    create_command_buffers();

    create_synchronization_primitives();

    setup_depth_stencil();

    setup_render_pass();

    // create_pipeline_cache();

    setup_framebuffer();

    // load device extension functions
    pfn_vkCmdSetPrimitiveTopology = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(
        vkGetInstanceProcAddr(*instance, "vkCmdSetPrimitiveTopologyEXT"));
    if (!pfn_vkCmdSetPrimitiveTopology)
        throw std::runtime_error("Failed to load extended dynamic state extensions");
    return;
}

void mv::MWindow::init_vulkan(void)
{
    // creates vulkan instance with specified instance extensions/layers
    create_instance();

    std::vector<vk::PhysicalDevice> physical_devices = instance->enumeratePhysicalDevices();

    if (physical_devices.size() < 1)
        throw std::runtime_error("No physical devices found");

    // Select device
    physical_device = std::make_unique<vk::PhysicalDevice>(std::move(physical_devices.at(0)));
    // resize devices container
    physical_devices.erase(physical_devices.begin());

    // get device info
    physical_properties = physical_device->getProperties();
    physical_features = physical_device->getFeatures();
    physical_memory_properties = physical_device->getMemoryProperties();

    std::vector<std::string> tmp;
    for (const auto &l : requested_device_extensions)
    {
        std::cout << "\t[-] Requesting device extension => " << l << "\n";
        tmp.push_back(l);
    }

    // create logical device handler mv::Device
    mv_device = std::make_unique<mv::Device>(*physical_device, tmp);

    // create logical device & graphics queue
    mv_device->create_logical_device(*physical_device);

    // get format
    depth_format = mv_device->get_supported_depth_format(*physical_device);

    // no longer pass references to swapchain, pass reference on per function basis now
    // swapchain->map(std::weak_ptr<vk::Instance>(instance),
    //                std::weak_ptr<vk::PhysicalDevice>(physical_device),
    //                std::weak_ptr<mv::Device>(mv_device));

    // Create synchronization objects
    vk::SemaphoreCreateInfo semaphore_info;
    semaphores->present_complete = mv_device->logical_device->createSemaphore(semaphore_info);
    semaphores->render_complete = mv_device->logical_device->createSemaphore(semaphore_info);

    return;
}

void mv::MWindow::create_instance(void)
{
    // Ensure we have validation layers
#ifndef NDEBUG
    check_validation_support();
#endif

    // Ensure we have all requested instance extensions
    check_instance_ext();

    vk::ApplicationInfo app_info;
    app_info.pApplicationName = "Bloody Day";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Moogin";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 2, 0);

/* If debugging enabled */
#ifndef NDEBUG
    vk::DebugUtilsMessengerCreateInfoEXT debugger_settings{};
    debugger_settings.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    debugger_settings.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                    vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    debugger_settings.pfnUserCallback = debug_message_processor;
    debugger_settings.pUserData = nullptr;

    // convert string request to const char*
    std::vector<const char *> req_layer;
    for (auto &layer : requested_validation_layers)
    {
        std::cout << "\t[-] Requesting layer => " << layer << "\n";
        req_layer.push_back(layer);
    }
    std::vector<const char *> req_inst_ext;
    for (auto &ext : f_req)
    {
        std::cout << "\t[-] Requesting instance extension => " << ext << "\n";
        req_inst_ext.push_back(ext.c_str());
    }

    vk::InstanceCreateInfo create_info;
    create_info.pNext = &debugger_settings;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(req_layer.size());
    create_info.ppEnabledLayerNames = req_layer.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(req_inst_ext.size());
    create_info.ppEnabledExtensionNames = req_inst_ext.data();
#endif
#ifdef NDEBUG /* Debugging disabled */
    std::vector<const char *> req_inst_ext;
    for (auto &ext : f_req)
    {
        std::cout << "\t[-] Requesting instance extension => " << ext << "\n";
        req_inst_ext.push_back(ext.c_str());
    }

    vk::InstanceCreateInfo create_info = {};
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(req_inst_ext.size());
    create_info.ppEnabledExtensionNames = req_inst_ext.data();
#endif

    instance = std::make_unique<vk::Instance>(vk::createInstance(create_info));
    // double check instance(prob a triple check at this point)
    if (!*instance)
        throw std::runtime_error("Failed to create vulkan instance");

    return;
}

void mv::MWindow::create_command_buffers(void)
{
    command_buffers->resize(swapchain->images->size());

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = *command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = static_cast<uint32_t>(command_buffers->size());

    *command_buffers = mv_device->logical_device->allocateCommandBuffers(alloc_info);

    if (command_buffers->size() < 1)
        throw std::runtime_error("Failed to allocate command buffers");
    return;
}

void mv::MWindow::create_synchronization_primitives(void)
{
    vk::FenceCreateInfo fence_info;
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    wait_fences->resize(swapchain->images->size(), nullptr);
    in_flight_fences->resize(MAX_IN_FLIGHT);

    for (auto &fence : *in_flight_fences)
    {
        fence = mv_device->logical_device->createFence(fence_info);
    }
    return;
}

void mv::MWindow::setup_depth_stencil(void)
{
    vk::ImageCreateInfo image_ci;
    image_ci.imageType = vk::ImageType::e2D;
    image_ci.format = depth_format;
    image_ci.extent = vk::Extent3D{window_width, window_height, 1};
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = vk::SampleCountFlagBits::e1;
    image_ci.tiling = vk::ImageTiling::eOptimal;
    image_ci.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

    // create depth stencil testing image
    depth_stencil->image = mv_device->logical_device->createImage(image_ci);

    // Allocate memory for image
    vk::MemoryRequirements mem_req;
    mem_req = mv_device->logical_device->getImageMemoryRequirements(depth_stencil->image);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex =
        mv_device->get_memory_type(mem_req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

    // allocate depth stencil image memory
    depth_stencil->mem = mv_device->logical_device->allocateMemory(alloc_info);

    // bind image and memory
    mv_device->logical_device->bindImageMemory(depth_stencil->image, depth_stencil->mem, 0);

    // Create view into depth stencil testing image
    vk::ImageViewCreateInfo iv_info;
    iv_info.viewType = vk::ImageViewType::e2D;
    iv_info.image = depth_stencil->image;
    iv_info.format = depth_format;
    iv_info.subresourceRange.baseMipLevel = 0;
    iv_info.subresourceRange.levelCount = 1;
    iv_info.subresourceRange.baseArrayLayer = 0;
    iv_info.subresourceRange.layerCount = 1;
    iv_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

    // if physical device supports high enough format add stenciling
    if (depth_format >= vk::Format::eD16UnormS8Uint)
    {
        iv_info.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }

    depth_stencil->view = mv_device->logical_device->createImageView(iv_info);
    return;
}

void mv::MWindow::setup_render_pass(void)
{
    std::array<vk::AttachmentDescription, 2> attachments;
    // Color attachment
    attachments[0].format = swapchain->color_format;
    attachments[0].samples = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

    // Depth attachment
    attachments[1].format = depth_format;
    attachments[1].samples = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp = vk::AttachmentLoadOp::eClear;
    attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].initialLayout = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::AttachmentReference color_ref;
    color_ref.attachment = 0;
    color_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::AttachmentReference depth_ref;
    depth_ref.attachment = 1;
    depth_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass_desc;
    subpass_desc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &color_ref;
    subpass_desc.pDepthStencilAttachment = &depth_ref;
    subpass_desc.inputAttachmentCount = 0;
    subpass_desc.pInputAttachments = nullptr;
    subpass_desc.preserveAttachmentCount = 0;
    subpass_desc.pPreserveAttachments = nullptr;
    subpass_desc.pResolveAttachments = nullptr;

    std::array<vk::SubpassDependency, 2> dependencies;
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[0].dstAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
    dependencies[1].srcAccessMask =
        vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
    dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
    dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

    vk::RenderPassCreateInfo render_pass_info;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_desc;
    render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
    render_pass_info.pDependencies = dependencies.data();

    render_pass = std::make_unique<vk::RenderPass>(mv_device->logical_device->createRenderPass(render_pass_info));
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

void mv::MWindow::setup_framebuffer(void)
{
    std::array<vk::ImageView, 2> attachments;

    // each frame buffer uses same depth image
    attachments[1] = depth_stencil->view;

    vk::FramebufferCreateInfo framebuffer_info;
    framebuffer_info.renderPass = *render_pass;
    framebuffer_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    framebuffer_info.pAttachments = attachments.data();
    framebuffer_info.width = swapchain->swap_extent.width;
    framebuffer_info.height = swapchain->swap_extent.height;
    framebuffer_info.layers = 1;

    // Framebuffer per swap image
    frame_buffers->resize(static_cast<uint32_t>(swapchain->images->size()));
    for (size_t i = 0; i < frame_buffers->size(); i++)
    {
        // different image views for each frame buffer
        attachments[0] = swapchain->buffers->at(i).view;
        frame_buffers->at(i) = mv_device->logical_device->createFramebuffer(framebuffer_info);
    }
    return;
}

void mv::MWindow::check_validation_support(void)
{
    std::vector<vk::LayerProperties> inst_layers = vk::enumerateInstanceLayerProperties();

    if (inst_layers.size() == 0 && !requested_validation_layers.empty())
        throw std::runtime_error("No supported validation layers found");

    // look for missing requested layers
    std::string prelude = "The following instance layers were not found...\n";
    std::string failed;
    for (const auto &requested_layer : requested_validation_layers)
    {
        bool match = false;
        for (const auto &layer : inst_layers)
        {
            if (strcmp(requested_layer, layer.layerName) == 0)
            {
                match = true;
                break;
            }
        }
        if (!match)
        {
            failed += requested_layer;
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

void mv::MWindow::check_instance_ext(void)
{
    std::vector<vk::ExtensionProperties> inst_extensions = vk::enumerateInstanceExtensionProperties();

    // use f_req vector for instance extensions
    if (inst_extensions.size() < 1 && !f_req.empty())
        throw std::runtime_error("No instance extensions found");

    std::string prelude = "The following instance extensions were not found...\n";
    std::string failed;
    for (const auto &requested_extension : f_req)
    {
        bool match = false;
        for (const auto &available_extension : inst_extensions)
        {
            if (strcmp(requested_extension.c_str(), available_extension.extensionName) == 0)
            {
                match = true;
                break;
            }
        }
        if (!match)
        {
            failed += requested_extension;
            failed += "\n";
        }
    }

    if (!failed.empty())
    {
        throw std::runtime_error(prelude + failed);
    }

    return;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_message_processor(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                       [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                                       [[maybe_unused]] void *user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Warning: " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
        std::cout << oss.str();
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT && (1 == 2)) // skip verbose messages
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
            << "Error: " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
        std::cout << oss.str();
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        std::ostringstream oss;
        oss << std::endl
            << "Info: " << callback_data->messageIdNumber << ", " << callback_data->pMessageIdName << std::endl
            << callback_data->pMessage << std::endl
            << std::endl;
    }
    return false;
}

std::vector<char> mv::MWindow::read_file(std::string filename)
{
    size_t file_size;
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
        file_size = (size_t)file.tellg();
        buffer.resize(file_size);

        // go back to beginning of file and read in
        file.seekg(0);
        file.read(buffer.data(), file_size);
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

vk::ShaderModule mv::MWindow::create_shader_module(const std::vector<char> &code)
{
    vk::ShaderModuleCreateInfo module_info;
    module_info.codeSize = code.size();
    module_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    vk::ShaderModule module = mv_device->logical_device->createShaderModule(module_info);

    return module;
}
