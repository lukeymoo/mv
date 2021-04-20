#include "mvSwap.h"

void mv::Swap::cleanup(bool should_detroy_surface)
{
    if (swapchain != nullptr)
    {
        for (uint32_t i = 0; i < image_count; i++)
        {
            if (buffers[i].view)
            {
                vkDestroyImageView(device, buffers[i].view, nullptr);
                buffers[i].view = nullptr;
            }
        }
        fp_destroy_swapchain_khr(device, swapchain, nullptr);
        swapchain = nullptr;
    }

    if (should_detroy_surface)
    {
        if (surface != nullptr)
        {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = nullptr;
        }
    }
    swapchain = nullptr;
    return;
}

void mv::Swap::init_surface(Display *disp, Window &window)
{
    VkXlibSurfaceCreateInfoKHR surface_info = {};
    surface_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    surface_info.dpy = disp;
    surface_info.window = window;
    this->display = disp;
    this->window = window;
    if (vkCreateXlibSurfaceKHR(instance, &surface_info, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create xcb surface");
    }

    // get queue family prop
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, nullptr);
    assert(queue_count >= 1);

    std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, queue_properties.data());

    // iterate queues, look for present support
    std::vector<VkBool32> supports_present(queue_count);
    for (uint32_t i = 0; i < queue_count; i++)
    {
        fp_get_physical_device_surface_support_khr(physical_device, i, surface, &supports_present[i]);
    }

    uint32_t graphics_index = UINT32_MAX;
    uint32_t present_index = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; i++)
    {
        if ((queue_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (graphics_index == UINT32_MAX)
            {
                graphics_index = i;
            }
            if (supports_present[i] == VK_TRUE)
            {
                present_index = i;
                break;
            }
        }
    }
    // just get separate queue
    if (present_index == UINT32_MAX)
    {
        for (uint32_t i = 0; i < queue_count; i++)
        {
            if (supports_present[i] == VK_TRUE)
            {
                present_index = i;
            }
        }
    }

    // failed to find queues
    if (graphics_index == UINT32_MAX || present_index == UINT32_MAX)
    {
        throw std::runtime_error("Failed to find graphics AND present support");
    }

    // dont support separate queue operations
    if (graphics_index != present_index)
    {
        throw std::runtime_error("This app does not support separate graphics and present queues yet");
    }

    queue_index = graphics_index;

    // get supported surface formats
    uint32_t surface_format_count = 0;
    fp_get_physical_device_surface_formats_khr(physical_device, surface, &surface_format_count, nullptr);
    assert(surface_format_count > 0);

    std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    fp_get_physical_device_surface_formats_khr(physical_device, surface, &surface_format_count, surface_formats.data());
    if ((surface_format_count == 1) && (surface_formats[0].format == VK_FORMAT_UNDEFINED))
    {
        color_format = VK_FORMAT_B8G8R8A8_UNORM;
        color_space = surface_formats[0].colorSpace;
    }
    else
    {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for (auto &&surface_format : surface_formats)
        {
            if (surface_format.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                color_format = surface_format.format;
                color_space = surface_format.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if (!found_B8G8R8A8_UNORM)
        {
            color_format = surface_formats[0].format;
            color_space = surface_formats[0].colorSpace;
        }
    }

    return;
}

void mv::Swap::connect(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device)
{
    this->instance = instance;
    this->physical_device = physical_device;
    this->device = device;

    // load fp
    fp_get_physical_device_surface_support_khr = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    fp_get_physical_device_surface_capabilities_khr = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    fp_get_physical_device_surface_formats_khr = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    fp_get_physical_device_surface_present_modes_khr = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

    fp_create_swapchain_khr = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    fp_destroy_swapchain_khr = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    fp_get_swapchain_images_khr = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
    fp_acquire_next_image_khr = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    fp_queue_present_khr = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
    return;
}

void mv::Swap::create(uint32_t *w, uint32_t *h)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    if (fp_get_physical_device_surface_capabilities_khr(physical_device, surface, &capabilities) != VK_SUCCESS)
    {
        throw std::runtime_error("Error getting surface capabilities");
    }

    // get present modes
    uint32_t present_count = 0;
    if (fp_get_physical_device_surface_present_modes_khr(physical_device, surface, &present_count, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Error getting count of present modes");
    }
    assert(present_count > 0);
    std::vector<VkPresentModeKHR> present_modes(present_count);
    if (fp_get_physical_device_surface_present_modes_khr(physical_device, surface, &present_count, present_modes.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retreive list of present modes");
    }

    Window rw;
    int rx, ry;           // returned x/y
    uint rwidth, rheight; // returned width/height
    uint rborder;         // returned  border width
    uint rdepth;

    // Prefer to configure extent from a direct response from x server
    if (XGetGeometry(display,
                     window, &rw, &rx, &ry,
                     &rwidth, &rheight, &rborder, &rdepth))
    {
        *w = static_cast<uint32_t>(rwidth);
        *h = static_cast<uint32_t>(rheight);
        swap_extent.width = *w;
        swap_extent.height = *h;
        std::cout << "[+] Swap extent configured by x server response" << std::endl
                  << "\tWidth -> " << *w << std::endl
                  << "\tHeight-> " << *h << std::endl;
    }
    else // Use the extent given by vulkan surface capabilities check earlier
    {
        *w = capabilities.currentExtent.width;
        *h = capabilities.currentExtent.height;
        swap_extent.width = *w;
        swap_extent.height = *h;
        std::cout << "[+] Swap extent defaulting to vulkan reported extent" << std::endl
                  << "Width -> " << *w << std::endl
                  << "Height-> " << *h << std::endl;
    }

    VkPresentModeKHR selected_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    // determine number of swap images
    uint32_t requested_image_count = capabilities.minImageCount + 1;
    // ensure not exceeding limit
    if ((capabilities.maxImageCount > 0) && (requested_image_count > capabilities.maxImageCount))
    {
        requested_image_count = capabilities.maxImageCount;
    }

    // get transform
    VkSurfaceTransformFlagsKHR selected_transform;
    selected_transform = capabilities.currentTransform;
    // if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    // {
    //     transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    // }
    // else
    // {
    //     transform = capabilities.currentTransform;
    // }

    VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // default ignore alpha(A = 1.0f)

    std::vector<VkCompositeAlphaFlagBitsKHR> composite_alpha_flags = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto &flag : composite_alpha_flags)
    {
        if (capabilities.supportedCompositeAlpha & flag)
        {
            composite_alpha = flag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swap_ci{};
    swap_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_ci.surface = surface;
    swap_ci.minImageCount = requested_image_count;
    swap_ci.imageFormat = color_format;
    swap_ci.imageColorSpace = color_space;
    swap_ci.imageExtent = {swap_extent.width, swap_extent.height};
    swap_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_ci.preTransform = (VkSurfaceTransformFlagBitsKHR)selected_transform;
    swap_ci.imageArrayLayers = 1;
    swap_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_ci.queueFamilyIndexCount = 0; // only matters when concurrent sharing mode
    swap_ci.presentMode = selected_present_mode;
    swap_ci.clipped = VK_TRUE;
    swap_ci.compositeAlpha = composite_alpha;

    if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        swap_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        swap_ci.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (fp_create_swapchain_khr(device, &swap_ci, nullptr, &swapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain");
    }

    // get swap images
    if (fp_get_swapchain_images_khr(device, swapchain, &image_count, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query swap chain image count");
    }
    assert(image_count > 0);

    images.resize(image_count);
    if (fp_get_swapchain_images_khr(device, swapchain, &image_count, images.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retreive swap chain images");
    }

    // create our swapchain buffer objs representing swap image & swap image view
    buffers.resize(image_count);
    for (uint32_t i = 0; i < image_count; i++)
    {
        VkImageViewCreateInfo view_ci{};
        view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_ci.pNext = nullptr;
        view_ci.format = color_format;
        view_ci.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A}; // VK_COMPONENT_SWIZZLE_IDENTITY i think
        view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_ci.subresourceRange.baseMipLevel = 0;
        view_ci.subresourceRange.levelCount = 1;
        view_ci.subresourceRange.baseArrayLayer = 0;
        view_ci.subresourceRange.layerCount = 1;
        view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_ci.flags = 0;

        buffers[i].image = images[i];
        view_ci.image = buffers[i].image;
        if (vkCreateImageView(device, &view_ci, nullptr, &buffers[i].view) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create view into swapchain image");
        }
    }
    return;
}