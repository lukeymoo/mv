#include "mvWindow.h"

void mv::MWindow::prepare(void)
{
    // initialize vulkan
    if (!init_vulkan())
    {
        throw std::runtime_error("Failed to initialize Vulkan");
    }

    swapchain->init(std::weak_ptr<xcb_connection_t>(xcb_conn),
                           std::weak_ptr<xcb_window_t>(xcb_win));
    command_pool = std::make_shared<vk::CommandPool>(mv_device->create_command_pool(swapchain->graphics_index));
    swapchain->create(&window_width, &window_height);
    create_command_buffers();
    create_synchronization_primitives();
    setup_depth_stencil();
    setup_render_pass();
    create_pipeline_cache();
    setup_framebuffer();
    return;
}

bool mv::MWindow::init_vulkan(void)
{
    // Create instance
    if (create_instance() != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create vulkan instance");
    }

    uint32_t device_count = 0;
    if (vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate physical devices count");
    }
    if (device_count == 0)
    {
        throw std::runtime_error("No adapters found on system");
    }
    std::vector<VkPhysicalDevice> physical_devices(device_count);
    if (vkEnumeratePhysicalDevices(m_instance, &device_count, physical_devices.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate physical device list");
    }

    // Select device
    m_physical_device = physical_devices[0];

    // Get device info
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);
    vkGetPhysicalDeviceFeatures(m_physical_device, &features);
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

    device = new mv::Device(m_physical_device);
    if (device->create_logical_device(features, requested_device_extensions) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device");
    }

    m_device = device->device;

    // get format
    depth_format = device->get_supported_depth_format(m_physical_device);

    // get graphics queue
    vkGetDeviceQueue(m_device, device->queue_family_indices.graphics, 0, &m_graphics_queue);
    device->graphics_queue = m_graphics_queue; // ensure device obj has handle to graphics queue

    swapchain.connect(m_instance, m_physical_device, m_device);

    // Create synchronization objects
    VkSemaphoreCreateInfo semaphore_info = mv::initializer::semaphore_create_info();
    if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &semaphores.present_complete) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create present semaphore");
    }
    if (vkCreateSemaphore(m_device, &semaphore_info, nullptr, &semaphores.render_complete) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render semaphore");
    }

    // submit info obj
    submit_info = mv::initializer::submit_info();
    submit_info.pWaitDstStageMask = &stage_flags;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &semaphores.present_complete;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &semaphores.render_complete;

    return true;
}

VkResult mv::MWindow::create_instance(void)
{
    // Ensure we have validation layers
#ifndef NDEBUG
    check_validation_support();
#endif

    // Ensure we have all requested instance extensions
    check_instance_ext();

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "Bloody Day";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Moogin";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_MAKE_VERSION(1, 2, 0);

/* If debugging enabled */
#ifndef NDEBUG
    VkDebugUtilsMessengerCreateInfoEXT debugger_settings{};
    debugger_settings.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugger_settings.pNext = nullptr;
    debugger_settings.flags = 0;
    debugger_settings.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugger_settings.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugger_settings.pfnUserCallback = debug_message_processor;
    debugger_settings.pUserData = nullptr;

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = &debugger_settings;
    create_info.flags = 0;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(requested_validation_layers.size());
    create_info.ppEnabledLayerNames = requested_validation_layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(requested_instance_extensions.size());
    create_info.ppEnabledExtensionNames = requested_instance_extensions.data();
#endif
#ifdef NDEBUG /* Debugging disabled */
    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = nullptr;
    create_info.flags = 0;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledLayerCount = static_cast<uint32_t>(requested_validation_layers.size());
    create_info.ppEnabledLayerNames = requested_validation_layers.data();
    create_info.enabledExtensionCount = static_cast<uint32_t>(requested_instance_extensions.size());
    create_info.ppEnabledExtensionNames = requested_instance_extensions.data();
#endif

    return vkCreateInstance(&create_info, nullptr, &m_instance);
}

void mv::MWindow::create_command_buffers(void)
{
    command_buffers.resize(swapchain.image_count);

    VkCommandBufferAllocateInfo alloc_info = mv::initializer::command_buffer_allocate_info(m_command_pool,
                                                                                           VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                                                           static_cast<uint32_t>(command_buffers.size()));
    if (vkAllocateCommandBuffers(m_device, &alloc_info, command_buffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers");
    }
    return;
}

void mv::MWindow::create_synchronization_primitives(void)
{
    VkFenceCreateInfo fence_info = mv::initializer::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    wait_fences.resize(swapchain.image_count, VK_NULL_HANDLE);

    in_flight_fences.resize(MAX_IN_FLIGHT);
    for (auto &fence : in_flight_fences)
    {
        if (vkCreateFence(m_device, &fence_info, nullptr, &fence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create in flight fence");
        }
    }
    return;
}

void mv::MWindow::setup_depth_stencil(void)
{
    // Create depth test image
    VkImageCreateInfo image_ci{};
    image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = depth_format;
    image_ci.extent = {window_width, window_height, 1};
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 1;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    if (vkCreateImage(m_device, &image_ci, nullptr, &depth_stencil.image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth stencil image");
    }

    // Allocate memory for image
    VkMemoryRequirements mem_req = {};
    vkGetImageMemoryRequirements(m_device, depth_stencil.image, &mem_req);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = device->get_memory_type(mem_req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device, &alloc_info, nullptr, &depth_stencil.mem) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate memory for depth stencil image");
    }
    if (vkBindImageMemory(m_device, depth_stencil.image, depth_stencil.mem, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to bind depth stencil image and memory");
    }

    // Create view
    VkImageViewCreateInfo iv_info = {};
    iv_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    iv_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    iv_info.image = depth_stencil.image;
    iv_info.format = depth_format;
    iv_info.subresourceRange.baseMipLevel = 0;
    iv_info.subresourceRange.levelCount = 1;
    iv_info.subresourceRange.baseArrayLayer = 0;
    iv_info.subresourceRange.layerCount = 1;
    iv_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    // for stencil + depth formats
    if (depth_format >= VK_FORMAT_D16_UNORM_S8_UINT)
    {
        iv_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (vkCreateImageView(m_device, &iv_info, nullptr, &depth_stencil.view) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth stencil image view");
    }
    return;
}

void mv::MWindow::setup_render_pass(void)
{
    std::array<VkAttachmentDescription, 2> attachments = {};
    // Color attachment
    attachments[0].format = swapchain.color_format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Depth attachment
    attachments[1].format = depth_format;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref = {};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref = {};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_desc = {};
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &color_ref;
    subpass_desc.pDepthStencilAttachment = &depth_ref;
    subpass_desc.inputAttachmentCount = 0;
    subpass_desc.pInputAttachments = nullptr;
    subpass_desc.preserveAttachmentCount = 0;
    subpass_desc.pPreserveAttachments = nullptr;
    subpass_desc.pResolveAttachments = nullptr;

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

    VkRenderPassCreateInfo render_pass_info = {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass_desc;
    render_pass_info.dependencyCount = static_cast<uint32_t>(dependencies.size());
    render_pass_info.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create render pass");
    }
    return;
}

void mv::MWindow::create_pipeline_cache(void)
{
    VkPipelineCacheCreateInfo pcinfo = {};
    pcinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (vkCreatePipelineCache(m_device, &pcinfo, nullptr, &m_pipeline_cache) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline cache");
    }
    return;
}

void mv::MWindow::setup_framebuffer(void)
{
    VkImageView attachments[2];

    attachments[1] = depth_stencil.view;

    VkFramebufferCreateInfo framebuffer_info = {};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = m_render_pass;
    framebuffer_info.attachmentCount = 2;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = swapchain.swap_extent.width;
    framebuffer_info.height = swapchain.swap_extent.height;
    framebuffer_info.layers = 1;

    // Framebuffer per swap image
    frame_buffers.resize(swapchain.image_count);
    for (size_t i = 0; i < frame_buffers.size(); i++)
    {
        attachments[0] = swapchain.buffers[i].view;
        if (vkCreateFramebuffer(m_device, &framebuffer_info, nullptr, &frame_buffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create frame buffer");
        }
    }
    return;
}

void mv::MWindow::check_validation_support(void)
{
    uint32_t layer_count = 0;
    if (vkEnumerateInstanceLayerProperties(&layer_count, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query supported instance layer count");
    }

    if (layer_count == 0 && !requested_validation_layers.empty())
    {
        throw std::runtime_error("No supported validation layers found");
    }

    std::vector<VkLayerProperties> available_layers(layer_count);
    if (vkEnumerateInstanceLayerProperties(&layer_count,
                                           available_layers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query supported instance layer list");
    }

    std::string prelude = "The following instance layers were not found...\n";
    std::string failed;
    for (const auto &requested_layer : requested_validation_layers)
    {
        bool match = false;
        for (const auto &available_layer : available_layers)
        {
            if (strcmp(requested_layer, available_layer.layerName) == 0)
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

void mv::MWindow::destroy_command_pool(void)
{
    if (m_command_pool != nullptr)
    {
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
        m_command_pool = nullptr;
    }
    return;
}

void mv::MWindow::check_instance_ext(void)
{
    uint32_t instance_extension_count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr,
                                               &instance_extension_count,
                                               nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query instance supported extension count");
    }

    if (instance_extension_count == 0 && !requested_instance_extensions.empty())
    {
        throw std::runtime_error("No instance level extensions supported by device");
    }

    std::vector<VkExtensionProperties> available_instance_extensions(instance_extension_count);
    if (vkEnumerateInstanceExtensionProperties(nullptr,
                                               &instance_extension_count,
                                               available_instance_extensions.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query instance supported extensions list");
    }

    std::string prelude = "The following instance extensions were not found...\n";
    std::string failed;
    for (const auto &requested_extension : requested_instance_extensions)
    {
        bool match = false;
        for (const auto &available_extension : available_instance_extensions)
        {
            // check if match
            if (strcmp(requested_extension, available_extension.extensionName) == 0)
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

VkShaderModule mv::MWindow::create_shader_module(const std::vector<char> &code)
{
    VkShaderModule module;

    VkShaderModuleCreateInfo module_info{};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.pNext = nullptr;
    module_info.flags = 0;
    module_info.codeSize = code.size();
    module_info.pCode = reinterpret_cast<const uint32_t *>(code.data());

    if (vkCreateShaderModule(m_device, &module_info, nullptr, &module) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module!");
    }

    return module;
}

inline XEvent mv::MWindow::create_event(const char *event_type)
{
    XEvent cev;

    cev.xclient.type = ClientMessage;
    cev.xclient.window = window;
    cev.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", true);
    cev.xclient.format = 32;
    cev.xclient.data.l[0] = XInternAtom(display, event_type, false);
    cev.xclient.data.l[1] = CurrentTime;

    return cev;
}

void mv::MWindow::handle_x_event(void)
{
    // count time for processing events
    XNextEvent(display, &event);
    mv::keyboard::key mv_key = mv::keyboard::key::invalid;

    // query if xinput
    if (event.xcookie.type == GenericEvent && event.xcookie.extension == xinput_opcode)
    {
        // if raw motion, query pointer
        XGetEventData(display, &event.xcookie);

        XGenericEventCookie *gen_cookie = &event.xcookie;
        XIDeviceEvent *xi_device_evt = static_cast<XIDeviceEvent *>(gen_cookie->data);
        // retrieve mouse deltas
        if (event.xcookie.evtype == XI_RawMotion)
        {
            std::cout << "Delta x,y => " << xi_device_evt->event_x << ", " << xi_device_evt->event_y << "\n";
            mouse->process_device_event(xi_device_evt);
        }
        XFreeEventData(display, &event.xcookie);
    }
    switch (event.type)
    {
    case LeaveNotify:
    case FocusOut:
        mouse->on_mouse_leave();
        XUngrabPointer(display, CurrentTime);
        break;
    case MapNotify:
    case EnterNotify:
        mouse->on_mouse_enter();
        // confine cursor to interior of window
        // mouse released on focus out or cursor leaving window
        XGrabPointer(display, window, 1, 0, GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
        break;
    case ButtonPress:
        if (event.xbutton.button == Button1)
        {
            mouse->on_left_press(event.xbutton.x, event.xbutton.y);
        }
        else if (event.xbutton.button == Button2)
        {
            mouse->on_middle_press(event.xbutton.x, event.xbutton.y);
        }
        else if (event.xbutton.button == Button3)
        {
            mouse->on_right_press(event.xbutton.x, event.xbutton.y);
        }
        // Mouse wheel scroll up
        else if (event.xbutton.button == Button4)
        {
            mouse->on_wheel_up(event.xbutton.x, event.xbutton.y);
        }
        // Mouse wheel scroll down
        else if (event.xbutton.button == Button5)
        {
            mouse->on_wheel_down(event.xbutton.x, event.xbutton.y);
        }
        break;
    case ButtonRelease:
        if (event.xbutton.button == Button1)
        {
            mouse->on_left_release(event.xbutton.x, event.xbutton.y);
        }
        else if (event.xbutton.button == Button2)
        {
            mouse->on_middle_release(event.xbutton.x, event.xbutton.y);
        }
        else if (event.xbutton.button == Button3)
        {
            mouse->on_right_release(event.xbutton.x, event.xbutton.y);
        }
        break;
    case KeyPress:
        // try to convert to our mv::keyboard::key enum
        mv_key = kbd->x11_to_mvkey(display, event.xkey.keycode);
        // check quit case
        if (mv_key == mv::keyboard::key::escape)
        {
            XEvent q = create_event("WM_DELETE_WINDOW");
            XSendEvent(display, window, false, ExposureMask, &q);
        }

        if (mv_key)
        {
            // send event only if not already signaled in key_states bitset
            if (!kbd->is_keystate(mv_key))
            {
                kbd->on_key_press(mv_key);
            }
        }
        break;
    case KeyRelease:
        mv_key = kbd->x11_to_mvkey(display, event.xkey.keycode);
        if (mv_key)
        {
            // x11 configured to not send repeat releases with xkb method in constructor
            // so just process it
            kbd->on_key_release(mv_key);
        }
        break;
    case Expose:
        break;
    case ClientMessage:
        std::cout << "[+] Exit requested" << std::endl;
        running = false;
        break;
    default: // Unhandled events do nothing
        break;
    } // End of switch
    return;
}