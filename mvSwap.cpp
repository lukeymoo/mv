#include "mvSwap.h"

void mv::Swap::cleanup(void)
{
    if (swapchain != nullptr)
    {
        for (uint32_t i = 0; i < imageCount; i++)
        {
            vkDestroyImageView(device, buffers[i].view, nullptr);
        }
    }
    if(surface != nullptr)
    {
        fpDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    swapchain = nullptr;
    surface = nullptr;
    return;
}

void mv::Swap::initSurface(xcb_connection_t *conn, xcb_window_t &window)
{
    VkXcbSurfaceCreateInfoKHR surfaceInfo = {};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.connection = conn;
    surfaceInfo.window = window;
    this->conn = conn;
    this->window = window;
    if (vkCreateXcbSurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create xcb surface");
    }

    // get queue family prop
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, nullptr);
    assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProperties(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, queueProperties.data());

    // iterate queues, look for present support
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++)
    {
        fpGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresent[i]);
    }

    uint32_t graphicsIndex = UINT32_MAX;
    uint32_t presentIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++)
    {
        if ((queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (graphicsIndex == UINT32_MAX)
            {
                graphicsIndex = i;
            }
            if (supportsPresent[i] == VK_TRUE)
            {
                presentIndex = i;
                break;
            }
        }
    }
    // just get separate queue
    if (presentIndex == UINT32_MAX)
    {
        for (uint32_t i = 0; i < queueCount; i++)
        {
            if (supportsPresent[i] == VK_TRUE)
            {
                presentIndex = i;
            }
        }
    }

    // failed to find queues
    if (graphicsIndex == UINT32_MAX || presentIndex == UINT32_MAX)
    {
        throw std::runtime_error("Failed to find graphics AND present support");
    }

    // dont support separate queue operations
    if (graphicsIndex != presentIndex)
    {
        throw std::runtime_error("This app does not support separate graphics and present queues yet");
    }

    queueIndex = graphicsIndex;

    // get supported surface formats
    uint32_t surfaceFormatCount = 0;
    fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr);
    assert(surfaceFormatCount > 0);

    std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatCount);
    fpGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats.data());
    if ((surfaceFormatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
    {
        colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
        colorSpace = surfaceFormats[0].colorSpace;
    }
    else
    {
        // iterate over the list of available surface format and
        // check for the presence of VK_FORMAT_B8G8R8A8_UNORM
        bool found_B8G8R8A8_UNORM = false;
        for (auto &&surfaceFormat : surfaceFormats)
        {
            if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                colorFormat = surfaceFormat.format;
                colorSpace = surfaceFormat.colorSpace;
                found_B8G8R8A8_UNORM = true;
                break;
            }
        }

        // in case VK_FORMAT_B8G8R8A8_UNORM is not available
        // select the first available color format
        if (!found_B8G8R8A8_UNORM)
        {
            colorFormat = surfaceFormats[0].format;
            colorSpace = surfaceFormats[0].colorSpace;
        }
    }

    return;
}

void mv::Swap::connect(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
{
    this->instance = instance;
    this->physicalDevice = physicalDevice;
    this->device = device;

    // load fp
    fpGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    fpGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    fpGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    fpGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

    fpCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    fpDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    fpGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(device, "vkGetSwapchainImagesKHR"));
    fpAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(device, "vkAcquireNextImageKHR"));
    fpQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(device, "vkQueuePresentKHR"));
    return;
}

void mv::Swap::create(uint32_t *w, uint32_t *h)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    if (fpGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities) != VK_SUCCESS)
    {
        throw std::runtime_error("Error getting surface capabilities");
    }

    // get present modes
    uint32_t presentCount = 0;
    if (fpGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Error getting count of present modes");
    }
    assert(presentCount > 0);
    std::vector<VkPresentModeKHR> presentModes(presentCount);
    if (fpGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentCount, presentModes.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retreive list of present modes");
    }

    xcb_get_geometry_cookie_t cookie;
    xcb_get_geometry_reply_t *reply;

    // Prefer to configure extent from a direct response from x server
    cookie = xcb_get_geometry(conn, window);
    if ((reply = xcb_get_geometry_reply(conn, cookie, NULL)))
    {
        *w = reply->width;
        *h = reply->height;
        swapExtent.width = *w;
        swapExtent.height = *h;
        std::cout << "[+] Swap extent configured by x server response" << std::endl
                  << "Width -> " << *w << std::endl
                  << "Height-> " << *h << std::endl;
    }
    else // Use the extent given by vulkan surface capabilities check earlier
    {
        *w = capabilities.currentExtent.width;
        *h = capabilities.currentExtent.height;
        swapExtent.width = *w;
        swapExtent.height = *h;
        std::cout << "[+] Swap extent defaulting to vulkan reported extent" << std::endl
                  << "Width -> " << *w << std::endl
                  << "Height-> " << *h << std::endl;
    }
    free(reply);

    VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    // determine number of swap images
    uint32_t requestedImageCount = capabilities.minImageCount + 1;
    // ensure not exceeding limit
    if ((capabilities.maxImageCount > 0) && (requestedImageCount > capabilities.maxImageCount))
    {
        requestedImageCount = capabilities.maxImageCount;
    }

    // get transform
    VkSurfaceTransformFlagsKHR selectedTransform;
    selectedTransform = capabilities.currentTransform;
    // if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    // {
    //     transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    // }
    // else
    // {
    //     transform = capabilities.currentTransform;
    // }

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // default ignore alpha(A = 1.0f)

    std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
    };
    for (auto &flag : compositeAlphaFlags)
    {
        if (capabilities.supportedCompositeAlpha & flag)
        {
            compositeAlpha = flag;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapCI{};
    swapCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapCI.surface = surface;
    swapCI.minImageCount = requestedImageCount;
    swapCI.imageFormat = colorFormat;
    swapCI.imageColorSpace = colorSpace;
    swapCI.imageExtent = {swapExtent.width, swapExtent.height};
    swapCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapCI.preTransform = (VkSurfaceTransformFlagBitsKHR)selectedTransform;
    swapCI.imageArrayLayers = 1;
    swapCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapCI.queueFamilyIndexCount = 0; // only matters when concurrent sharing mode
    swapCI.presentMode = selectedPresentMode;
    swapCI.clipped = VK_TRUE;
    swapCI.compositeAlpha = compositeAlpha;

    if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
    {
        swapCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }

    if (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
    {
        swapCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (fpCreateSwapchainKHR(device, &swapCI, nullptr, &swapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain");
    }

    // get swap images
    if (fpGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query swap chain image count");
    }
    assert(imageCount > 0);

    images.resize(imageCount);
    if (fpGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retreive swap chain images");
    }

    // create our swapchain buffer objs representing swap image & swap image view
    buffers.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++)
    {
        VkImageViewCreateInfo viewCI{};
        viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCI.pNext = nullptr;
        viewCI.format = colorFormat;
        viewCI.components = {
            VK_COMPONENT_SWIZZLE_R,
            VK_COMPONENT_SWIZZLE_G,
            VK_COMPONENT_SWIZZLE_B,
            VK_COMPONENT_SWIZZLE_A}; // VK_COMPONENT_SWIZZLE_IDENTITY i think
        viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCI.subresourceRange.baseMipLevel = 0;
        viewCI.subresourceRange.levelCount = 1;
        viewCI.subresourceRange.baseArrayLayer = 0;
        viewCI.subresourceRange.layerCount = 1;
        viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCI.flags = 0;

        buffers[i].image = images[i];
        viewCI.image = buffers[i].image;
        if (vkCreateImageView(device, &viewCI, nullptr, &buffers[i].view) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create view into swapchain image");
        }
    }
    return;
}